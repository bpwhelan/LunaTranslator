import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import path from 'node:path';
import { setTimeout as delay } from 'node:timers/promises';

const [directory, bits] = process.argv.slice(2);
assert(directory && /^(32|64)$/.test(bits), 'Usage: node test_gsm_lunahook.mjs <binary-directory> <32|64>');
const cwd = path.resolve(directory);
const children = [];
function start(name, encoding) {
    const child = spawn(path.join(cwd, name), [], { cwd, windowsHide: true, stdio: 'pipe' });
    children.push(child);
    child.output = '';
    child.errors = '';
    child.stdout.setEncoding(encoding);
    child.stdout.on('data', (chunk) => { child.output += chunk; });
    child.stderr.on('data', (chunk) => { child.errors += chunk.toString(); });
    child.on('error', (error) => { child.failure = error; });
    return child;
}
async function waitFor(child, predicate, label, timeout = 25000) {
    const end = Date.now() + timeout;
    do {
        if (child.failure) throw child.failure;
        if (predicate(child.output)) return;
        if (child.exitCode !== null) throw new Error(`${label}: exited ${child.exitCode}\n${child.output}\n${child.errors}`);
        await delay(50);
    } while (Date.now() < end);
    throw new Error(`${label}: timed out\n${child.output}\n${child.errors}`);
}
try {
    const fixture = start('GsmHookSmokeTarget.exe', 'utf8');
    await waitFor(fixture, (text) => /^\d+ [0-9A-Fa-f]+\r?\n/.test(text), 'fixture startup');
    const [, pid, address] = /^(\d+) ([0-9A-Fa-f]+)/.exec(fixture.output);
    const cli = start(`LunaHostCLI${bits}.exe`, 'utf16le');
    const command = (text) => cli.stdin.write(Buffer.from(`${text}\n`, 'utf16le'));
    await waitFor(cli, (text) => text.includes('Usage:'), 'CLI startup');
    command('invalid');
    command('showall');
    await waitFor(cli, (text) => text.includes('Invalid command:'), 'UTF-16 command input');
    command(`attach -P${pid}`);
    await waitFor(cli, (text) => text.includes(`Connected process ${pid}`), 'attach');
    command(`+50 -P${pid}`);
    command(`=932 -P${pid}`);
    command(`RQ@${address} -P${pid}`);
    await waitFor(cli, (text) => /^\[#[0-9A-F]+\|[^\]]+\] GSM LunaHook smoke test 日本語\r?$/m.test(text), 'GSM-compatible Japanese text');
    const match = /^\[#([0-9A-F]+)\|([^\]]+)\] GSM LunaHook smoke test 日本語\r?$/m.exec(cli.output);
    const fields = match[2].split(':');
    assert.equal(parseInt(fields[1], 16), Number(pid));
    assert.equal(BigInt(`0x${fields[2]}`), BigInt(`0x${address}`));
    assert(fields[5].length > 0, 'hook name is present');
    assert.equal(fields[6].toUpperCase(), `RQ@${address}`.toUpperCase().replace(/@0+/, '@'));
    command(`-${BigInt(`0x${address}`)} -P${pid}`);
    command(`detach -P${pid}`);
    await waitFor(cli, (text) => text.includes(`Disconnected process ${pid}`), 'detach');
    cli.stdin.end();
    await waitFor(cli, () => cli.exitCode === 0, 'clean EOF');
    assert.equal(fixture.exitCode, null, 'target survives detach');
    console.log(`PASS ${bits}-bit: UTF-16 commands, attach, Japanese text with GSM context, remove, detach, EOF`);
} finally {
    for (const child of children) if (child.exitCode === null) child.kill();
}
