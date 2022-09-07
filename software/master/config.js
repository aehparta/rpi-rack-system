const yaml = require('yaml');
const fs = require('fs');
const merge = require('deepmerge');

const configDefault = {
  url: 'http://rack:8000',
  bind: '0.0.0.0',
  port: 8000,
  slots: {
    0: { label: 'Master' },
    1: { label: 'Slot #1' },
    2: { label: 'Slot #2' },
    3: { label: 'Slot #3' },
    4: { label: 'Slot #4' },
    5: { label: 'Slot #5' },
    6: { label: 'Slot #6' },
    7: { label: 'Slot #7' },
    8: { label: 'Slot #8' },
    9: { label: 'Slot #9' },
    10: { label: 'Slot #10' },
    11: { label: 'Slot #11' },
  },
  spi: {
    speed: 500000,
  },
};

let config = { ...configDefault };
try {
  /* read config from file */
  const configFile = yaml.parse(fs.readFileSync('config.yml', 'utf-8'));
  if (configFile) {
    config = merge(config, configFile);
    if (typeof config.slots === 'object') {
      config.slots = Object.values(config.slots);
    }
    console.log(config);
  }
} catch (e) {}

module.exports = config;
