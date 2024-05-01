<?php
error_reporting(E_ALL);

set_time_limit(0);

// Turn on implicit output flushing so we see what we're getting as it comes in
ob_implicit_flush();

$address = '0.0.0.0';
$port = 4242;

if (($sock = socket_create(AF_INET, SOCK_STREAM, SOL_TCP)) === false) {
	echo "socket_create() failed: reason: " . socket_strerror(socket_last_error()) . "\n";
	exit(1);
}

if (socket_bind($sock, $address, $port) === false) {
	echo "socket_bind() failed: reason: " . socket_strerror(socket_last_error($sock)) . "\n";
	exit(1);
}

if (socket_listen($sock, 5) === false) {
	echo "socket_listen() failed: reason: " . socket_strerror(socket_last_error($sock)) . "\n";
	exit(1);
}

echo "Listening...\n";

try {
	do {
		if (($msgsock = socket_accept($sock)) === false) {
			echo "socket_accept() failed: reason: " . socket_strerror(socket_last_error($sock)) . "\n";
			break;
		}
		echo "Got connection\n";

		$recv = "";
		do {
			if (false === ($buf = socket_read($msgsock, 50, PHP_NORMAL_READ))) {
				echo "socket_read() failed: reason: " . socket_strerror(socket_last_error($msgsock)) . "\n";
			}
			//echo "$buf";
			$recv .= $buf;
			if (strstr($buf, "\n") === false) {
				continue;
			}
			//if (!$buf = trim($buf)) {
			//	continue;
			//}
			if ($buf == 'quit') {
				break;
			}
			$recv_data = json_decode($recv, true);
			print_r($recv_data);

			$date = date("Y-m-d H:i:s");
			$year = intval(date("Y"));
			$month = intval(date("m"));
			$day = intval(date("d"));
			$weekday = intval(date("w"));
			$hour = intval(date("H"));
			$minute = intval(date("i"));
			$second = intval(date("s"));
			$data = [
				"status" => 2,
				"date" => [
					"full" => $date,
					"year" => $year,
					"month" => $month,
					"day" => $day,
					"weekday" => $weekday,
					"hour" => $hour,
					"minute" => $minute,
					"second" => $second,
				],
			];
			$response = json_encode($data)."\n";
			socket_write($msgsock, $response, strlen($response));
			echo "Sent:[".trim($response)."]\n";
			break;
		} while (true);

		echo "Received:[".trim($recv)."]\n";

		echo "Disconnect\n";
		socket_close($msgsock);
	} while (true);
} catch (Exception $e) {
	echo "ERROR: ".$e->getMessage()."\n";
} finally {
	socket_close($sock);
}
