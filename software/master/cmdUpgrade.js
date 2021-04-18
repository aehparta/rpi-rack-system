const { spawn } = require('child_process');
const fs = require('fs');
const slots = require('./slots');
const { spi } = require('./io');

const binaryFileKeeperSlot = '../keepers/keeper-slot-AVR.elf';
const binaryFileKeeperFans = '../keepers/keeper-fans-AVR.elf';
const binaryFileBattery = '../keepers/battery-AVR.elf';

console.log('Checking binaries:');
try {
  fs.statSync(binaryFileKeeperSlot);
  fs.statSync(binaryFileKeeperFans);
  // fs.statSync(binaryFileBattery);
} catch (e) {
  console.log(e);
  process.exit(1);
}
console.log(' - binaries OK');

const upgrade = (slotId, single = false) => {
  const slot = slots[slotId];
  if (slot === undefined) {
    spi.select(15);
    return;
  }

  slot.select(() => {
    console.log(`### slot #${slot.id}: upgrading ###`);
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
      '-U',
      `flash:w:${binaryFileKeeperSlot}`,
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

console.log('Starting upgrade:');
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
