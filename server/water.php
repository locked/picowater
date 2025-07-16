<?php
require_once("Database.php");

$config = require_once("water.config.php");

$date = date("Y-m-d H:i:s");
$year = intval(date("Y"));
$month = intval(date("m"));
$day = intval(date("d"));
$weekday = intval(date("w"));
$hour = intval(date("H"));
$minute = intval(date("i"));
$second = intval(date("s"));

$devices = [
	"E663589863885324",
	"E663A837CB849426", // 2nd
];

$device_id = $_GET["id"];
$dist = $_GET["dist"];
$batlvl = $_GET["batlvl"];
$batlvl2 = $_GET["batlvl2"] ?? -1;
$systemp = $_GET["systemp"] ?? -1;
$date = $_GET["date"];

$db = new Database($config["mysql"]);

function getLast($db, $deviceid) {
	$rows = $db->select("SELECT MAX(datetime) as pump_latest FROM records WHERE deviceid = :deviceid AND pump > 0", ["deviceid" => $deviceid]);
	return $rows;
}

$sensor_config = $config["sensors"][$device_id] ?? null;
$max_dist = $sensor_config["max_dist"] ?? 25;
$min_batlvl = $sensor_config["min_batlvl"] ?? 2.8;

$remaining_dist = $max_dist - $dist;

$pump = 0;	// Default
switch ($device_id) {
	case "E663A837CB849426":
		if ($remaining_dist > 0 && $batlvl >= $min_batlvl) {
			$pump = in_array($hour, [7, 20]) ? 6000 : 0;
			//$pump = in_array($hour, [20]) ? 5000 : 0;
		}
	break;
	default:
		if ($remaining_dist > 0 && $batlvl >= $min_batlvl) {
			$dynamic_mode = true;
			if ($dynamic_mode) {
				if ($hour <= 11 || $hour >= 19) {
					// Fetch 'last' from db
					$rows = getLast($db, $device_id);
					$last = strtotime($rows[0]["pump_latest"] ?? "");
					$elapsed = time() - $last;
					if ($elapsed > 3600 * 6) {
						$pump = 16000;
					}
				}
			} else {
				$pump = in_array($hour, [7, 20, 22]) ? 15000 : 0;
			}
		}
}

error_log("PICOWATER device_id:[".$device_id."] dist:[".$dist."(rem:".$remaining_dist."/".$max_dist.")] batlvl:[".$batlvl."(min:".$min_batlvl.")] batlvl2:[".$batlvl2."] systemp:[".$systemp."] date:[".$date."] pump:[".$pump."] last:[".($last ?? "-")."]");

$data = [
	"status" => 0,
	"date" => [
		//"full" => $date,
		"year" => $year,
		"month" => $month,
		"day" => $day,
		"weekday" => $weekday,
		"hour" => $hour,
		"minute" => $minute,
		"second" => $second,
	],
	"wakeup_everymin" => 0,
	"pump" => $pump,
];
$response = json_encode($data)."\n";

echo $response;

$db->insert("INSERT INTO records (datetime, deviceid, batlvl, bat2lvl, dist, tempsys, pump) VALUES (NOW(), :deviceid, :batlvl, :bat2lvl, :dist, :tempsys, :pump)", [
	"deviceid" => $device_id,
	"batlvl" => $batlvl,
	"bat2lvl" => $batlvl2,
	"dist" => $dist,
	"tempsys" => $systemp,
	"pump" => $pump,
]);
