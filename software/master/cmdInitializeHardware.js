const { spawn } = require('child_process');
const slots = require('./slots');

const errors = [];

const initialize = (slot, callback) => {
  slot.select(() => {
    console.log(`### slot #${slot.id}: initializing ###`);
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
      '-b',
      '100000',
      '-U',
      'lfuse:w:0xde:m',
      '-U',
      'hfuse:w:0xd1:m',
      '-U',
      'efuse:w:0xff:m',
      '-U',
      `eeprom:w:${slot.id},0:m`,
    ];
    const cmd = spawn('sudo', args);
    cmd.stdout.on('data', (data) => process.stdout.write(data));
    cmd.stderr.on('data', (data) => process.stderr.write(data));
    cmd.on('close', (code) => {
      if (code !== 0) {
        errors.push([slot.id, 'failed']);
      }
      if (slots[slot.id + 1] !== undefined) {
        initialize(slots[slot.id + 1], callback);
      } else {
        callback();
      }
    });
  });
};

initialize(slots[0], () => {
  if (errors.length > 0) {
    console.log(errors.map((e) => `slot #${e[0]}: ${e[1]}`).join('\n'));
  }
});
