const { spawn } = require('child_process');
const slots = require('./slots');

const upgrade = (slotId, single = false) => {
  const slot = slots[slotId];
  if (slot === undefined) {
    return;
  }

  slot.select(() => {
    console.log(`### slot #${slot.id} ###`);
    const args = [
      'avrdude',
      '-q',
      '-q',
      '-p',
      'atmega328p',
      '-c',
      'linuxspi',
      '-P',
      '/dev/spidev0.0',
    ];
    const cmd = spawn('sudo', args);
    cmd.stdout.on('data', (data) => process.stdout.write(data));
    cmd.stderr.on('data', (data) => process.stderr.write(data));
    cmd.on('close', (code) => {
      code !== 0 && console.error(' - FAILED');
      if (!single) {
        upgrade(slotId + 1);
      }
    });
  });
};

console.log('Resetting slots:');
console.log('');

const slotId = process.argv?.[2] ? Number(process.argv[2]) : undefined;
if (
  slotId !== undefined &&
  (slotId < 0 || slotId >= slots.length || Number.isNaN(slotId))
) {
  console.error(`Invalid slot #${slotId}`);
  process.exit(1);
}

upgrade(slotId || 0, slotId !== undefined);
