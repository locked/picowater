<?php
require_once("Database.php");

$config = require_once("water.config.php");
$db = new Database($config["mysql"]);

$device_id = $_REQUEST["device_id"] ?? null;
if (empty($device_id)) {
	//$rows = $db->select("SELECT deviceid, MAX(datetime) as seen_latest FROM records GROUP BY deviceid");
	$rows = $db->select("SELECT deviceid, MAX(id) as max_id, sum(pump) / 1000 as sum_pump FROM records WHERE datetime > '2025-06-26 10:00:00' GROUP BY deviceid");
	echo "<table style='width:50%'>";
	echo "<tr style='text-align:left;'><th>deviceid</th><th>last seen</th><th>batlvl</th><th>bat2lvl</th><th>water lvl</th><th>tempsys</th><th>sum pump</th></tr>";
	foreach ($rows as $row) {
		$data = $db->select("SELECT * FROM records WHERE id = :id", ["id" => $row["max_id"]])[0];
		$sensor_config = $config["sensors"][trim($data["deviceid"])];
		$device_dist_max = $sensor_config["max_dist"];
		echo "<tr><td><a href=\"?device_id=".$data["deviceid"]."\">".$data["deviceid"]."</a></td><td>".$data["datetime"]."</td><td>".$data["batlvl"]."</td><td>".$data["bat2lvl"]."</td><td>".($device_dist_max - $data["dist"])." (".$device_dist_max." - ".$data["dist"].")</td><td>".$data["tempsys"]."</td><td>".intval($row["sum_pump"])."</td></tr>";
		// bat2lvl | dist | tempsys | pump
	}
	echo "</table>";
} else {
	$rows = $db->select("SELECT * FROM records WHERE deviceid = :deviceid", [
		"deviceid" => $device_id,
	]);
	echo "<table style=\"width:100%;text-align:left;\">";
	echo "<tr>";
	foreach ($rows[0] as $col => $v) {
		echo "<th>".$col."</th>";
	}
	echo "</tr>";
	foreach ($rows as $row) {
		echo "<tr>";
		foreach ($row as $col => $v) {
			echo "<td>".$v."</td>";
		}
		echo "</tr>";
	}
	echo "</table>";
}
