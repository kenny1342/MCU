<?PHP

$d = file_get_contents("http://192.168.30.80/json");
$obj = json_decode($d);

print_r($obj);

?>
