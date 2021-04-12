const slots = require('./slots');

const slotId = Number(process.argv?.[2]);
if (slotId < 0 || slotId >= slots.length || Number.isNaN(slotId)) {
  console.error(`Invalid slot #${slotId}`);
  process.exit(1);
}

slots[slotId].select();
