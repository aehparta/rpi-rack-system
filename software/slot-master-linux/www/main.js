var slot0 = {
	host: 'pl500w.lan',
	port: 80,
	basePort: 2000
};
var sn = 0;
var term = new Terminal({
	rows: 40
});

term.open(document.getElementById('terminal'));
term.write('trying to connect...\r\n');

var s = new WebSocket("ws://" + slot0.host + ":" + slot0.port + '/slot/' + 0);
s.onmessage = function(event) {
	event.data.arrayBuffer().then(function(buffer) {
		const data = new Uint8Array(buffer);
		for (var i = 0; i < data.length; i++) {
			// console.log(String.fromCharCode(data[i]));
			term.write(String.fromCharCode(data[i]));
		}
	});
	// console.log(event.data);
	// term.write(event.data);
}

term.onKey(function(data) {
	if (data.key == 'q') {
		term.write("closing\r\n");
		s.close();
	} else {
		console.log(s.readyState);
		s.send('hell');
		console.log(data.key);
	}
});