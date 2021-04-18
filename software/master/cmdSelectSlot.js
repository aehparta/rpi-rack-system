const {spi} = require('./io');

const slotId = Number(process.argv?.[2]);
if (slotId < 0 || slotId >= 16 || Number.isNaN(slotId)) {
  console.error(`Invalid slot #${slotId}`);
  process.exit(1);
}

spi.select(slotId);
