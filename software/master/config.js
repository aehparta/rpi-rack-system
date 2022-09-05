const yaml = require('yaml');
const fs = require('fs');

const configDefault = {
  url: 'http://rack:8000',
  bind: '0.0.0.0',
  port: 8000,
  slots: [
    { label: 'Master' },
    { label: 'Slot #1' },
    { label: 'Slot #2' },
    { label: 'Slot #3' },
    { label: 'Slot #4' },
    { label: 'Slot #5' },
    { label: 'Slot #6' },
    { label: 'Slot #7' },
    { label: 'Slot #8' },
    { label: 'Slot #9' },
    { label: 'Slot #10' },
    { label: 'Slot #11' },
  ],
  spi: {
    speed: 500000,
  },
};

let config = { ...configDefault };
try {
  /* read config from file */
  const config = yaml.parse(fs.readFileSync('config.yml', 'utf-8'));
  if (content) {
    config = { ...config, content };
  }
} catch (e) {}

module.exports = config;
