<?php
require_once ('jpgraph/jpgraph.php');
require_once ('jpgraph/jpgraph_line.php');
require_once ('jpgraph/jpgraph_date.php');
require_once("Database.php");

$config = require_once("water.config.php");
$db = new Database($config["mysql"]);

$rows = $db->select("SELECT substring(datetime, 1, 13) as dt, deviceid, batlvl, bat2lvl, dist, tempsys, pump FROM records WHERE datetime > '2025-06-26 10:00:00' ORDER BY datetime ASC", []);

$type = $_REQUEST["type"] ?? "dist";
$tmp_data = [];
$dts = [];
$metrics = [];
foreach ($rows as $row) {
	$dt = strtotime($row["dt"].":00");
	$sensor_config = $config["sensors"][trim($row["deviceid"])];
	$device_dist_max = $sensor_config["max_dist"];
	if ($type == "dist") {
		$row[$type] = $device_dist_max - $row[$type];
	}
	$metric = $row["deviceid"]."_".$type;
	$tmp_data[$metric][$dt] = $row[$type];
	$metrics[$metric] = $metric;
	$dts[] = $dt;
}
$data = [];
$labels = [];
$xdata = [];
foreach ($dts as $dt) {
	$date = date("Y-m-d H:i", $dt);
	foreach ($metrics as $metric) {
		$data[$metric][] = $tmp_data[$metric][$dt] ?? null;
	}
	$labels[] = $date;
	$xdata[] = $dt;
}
//print_r($data); exit();

// Setup the graph
$graph = new Graph(1200,800);
//$mode = "textlin";
$mode = "datlin";
$graph->SetScale($mode);

$theme_class = new UniversalTheme;

$graph->SetTheme($theme_class);
$graph->img->SetAntiAliasing(false);
$graph->title->Set($type);
$graph->SetBox(false);

$graph->SetMargin(40,20,36,115);

$graph->img->SetAntiAliasing();

$graph->yaxis->HideZeroLabel();
$graph->yaxis->HideLine(false);
$graph->yaxis->HideTicks(false,false);

$graph->xgrid->Show();
$graph->xgrid->SetLineStyle("solid");
$graph->xgrid->SetColor('#E3E3E3');

$graph->xaxis->SetTickLabels($labels);
$graph->xaxis->SetLabelAngle(90);
//$graph->xaxis->scale->SetGrace(null, 2);
//$graph->xaxis->SetLabelFormatString('M, Y',true);

// Create the first line
$i = 0;
foreach ($data as $sensor => $sensor_data) {
	$p1 = new LinePlot($sensor_data, $xdata);
	$graph->Add($p1);
	$p1->SetColor(match($i) {
		0 => "#6495ED",
		1 => "#B22222",
		default => "#000000",
	});
	$p1->SetLegend($sensor);
	$i++;
}

$graph->legend->SetFrameWeight(1);

// Output line
$graph->Stroke();
