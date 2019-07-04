<?php

namespace App\Presenters;

use Nette;
use Nette\Application\UI\Form;
use Nette\Utils\DateTime;

class GraphPresenter extends Nette\Application\UI\Presenter
{
	/** @var Nette\Database\Context */
	private $database;

	public function __construct(Nette\Database\Context $database)
	{
		$this->database = $database;
	}

	public function renderDefault($sensors = [], $value, $date_from, $date_to)
	{
		if (!$this->getUser()->isLoggedIn()) {
        		$this->redirect('Sign:in');
    		}
		$this->template->period = [];
		$this->template->period[0] = $date_from;
		$this->template->period[1] = $date_to;
		if (isset($sensors) && !empty($sensors) && isset($date_from) && isset($date_to) && isset($value)){
			$this->template->values = $this->database->table("valreal")
                                ->select("DATETIME(timestamp, ?) AS local_timestamp, value, sensor.name AS sensor_name", "localtime")
                                ->order("timestamp")
                                ->where('sensor_id', array_unique($sensors))
                                ->where('valname.name', $value)
                                ->where('timestamp > DATETIME(?,?,?)', DateTime::from($date_from)->getTimestamp(),'unixepoch', 'localtime')
				->where('timestamp < DATETIME(?,?,?)', DateTime::from($date_to)->getTimestamp(),'unixepoch', 'localtime');
                        $this->template->sensors =  $this->database->table("sensor")->where("id", array_unique($sensors));
			$this->template->units = $this->database->table("unit")
                                                        ->select("unit.name")
                                                        ->where(':valname.name', $value)
							->fetch()["name"];
			$this->template->value_type = $value;
			$result = [];
			foreach($this->template->sensors as $row){
				$result[$row['name']] = [];
			}
			foreach ($this->template->values as $row)
			{
				array_push ($result[$row['sensor_name']], [DateTime::from($row['local_timestamp'])->getTimestamp(), $row['value']]);
			}
			$this->template->average = [];
			$this->template->diff = [];
			foreach($result as $sensor_name => $sensor_values){
				$average = null;
				$min = PHP_INT_MIN;
                                $max = PHP_INT_MAX;
				if (count($sensor_values) > 1) {
					reset($sensor_values);
					$start = current($sensor_values)[0];
					$timebase = end($sensor_values)[0] - $start;
					$previous_value = null;
					$average = 0;
					$min = PHP_INT_MIN;
					$max = PHP_INT_MAX;
					foreach($sensor_values as $sensor_value) {
						if ($previous_value != null){
							$diff = ($sensor_value[0] - $previous_value[0]) / $timebase;
							$average += $previous_value[1] * $diff;
						}
						$previous_value = $sensor_value;
						if ($min == PHP_INT_MIN || $sensor_value[1] < $min)
							$min = $sensor_value[1];
						if ($max == PHP_INT_MAX || $sensor_value[1] > $max)
							$max = $sensor_value[1];
					}
				}
				array_push($this->template->average, $average);
				array_push($this->template->diff, $max - $min);
			}
		}
	}

	protected function createComponentGraphForm()
	{
		$httpRequest = $this->getHttpRequest();
		$form = new Form;
		$form->addDateTimePicker('date_from', 'Měření Od')
                  ->setFormat('m.d.Y H:i')
                  ->setRequired(TRUE)
                  ->setDefaultValue($httpRequest->getQuery("date_from"));
                $form->addDateTimePicker('date_to', 'Měření Do')
                  ->setFormat('m.d.Y H:i')
                  ->setRequired(TRUE)
                  ->setDefaultValue($httpRequest->getQuery("date_to"));
		// well this one is bad for performance but... consider it little problem right now
                // and if it proves bigger problem we will optimeze it somehow - I consider better backjoin or myabe two effective calls to different tables
		//$opt = $this->database->table("valreal")
		//	->select("sensor.id AS sensor_id, sensor.name AS sensor_name")
		//	->group("sensor_id")
		//	->where('valname.name', $httpRequest->getQuery("value"))
		//	->fetchPairs("sensor_id", "sensor_name");
		//this one was slower on my hw (you can benchmark it)
		//$opt1 = $this->database->table("sensor")
                //        ->where(':valreal.valname.name', $httpRequest->getQuery("value"))
                //        ->fetchPairs("id", "name");
		$opt = $this->database->table("valsensor")
			->where('valname.name', $httpRequest->getQuery("value"))
			->select("sensor_id, sensor.name AS sensor_name")
			->fetchPairs("sensor_id", "sensor_name");
		$checkboxlistg = $form->addCheckboxList('sensors', 'Senzory:', $opt);
		if ($httpRequest->getQuery("sensors") != null && !empty($httpRequest->getQuery("sensors"))) {
			 $checkboxlistg->setDefaultValue(array_unique($httpRequest->getQuery("sensors")));
		}
		$form->addHidden('value', $httpRequest->getQuery("value"));
		$form->addSubmit('send', 'Aktualizovat Graf');
		$form->setAction('default');
                $form->setMethod('GET');
		return $form;
	}
}
