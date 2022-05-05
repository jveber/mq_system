<?php

namespace App\Presenters;

use Nette;
use Nette\Application\UI\Form;
use Nette\Utils\DateTime;


class MyTranslator implements \Nette\Localization\ITranslator
{

    /**
     * @param $message
     * @param null $count
     * @return mixed
     */
    public function translate($message, ...$parameters): string
    {
        bdump($message);
        return $message;
    }
}

class HomepagePresenter extends Nette\Application\UI\Presenter
{
    /** @var Nette\Database\Context */
    private $data_db;       // contains sensor data
    private $log_db;        // contains logs from modules
    private $script_db;     // contains executable scripts for exe module

    public function __construct(Nette\Database\Context $database, Nette\Database\Context $log, Nette\Database\Context $script)
    {
        $this->data_db = $database;
        $this->log_db = $log;
        $this->script_db = $script;
    }

   function beforeRender()
   {
  	$this->template->setTranslator(new MyTranslator());
   }

    public function renderDefault()
    {
        if (!$this->getUser()->isLoggedIn()) {
            $this->redirect('Sign:in');
        }

        /******* Current Sensor Values request ********/
        $this->template->posts = $this->data_db->table("valsensor")
            ->order("sensor_id")
            ->select("DATETIME(timestamp, ?) AS timestamp, value, valname.name AS value_name, sensor.name AS sensor_name, valname.unit.name AS units_name, sensor_id", "localtime");
        $this->template->post_hash = array();
        foreach($this->template->posts as $post) // this may not be effective
            $this->template->post_hash[$post->value_name] = hash("md5", $post->value_name); 
 	// technically this is the slow alternative but I made some improvements in database to use table valsensor instead (of valreal) and it's much (much) faster.. because valsensor is much smaller than valreal and does not grow in time (so the request time is likely constant).
        /*  $this->template->posts = $this->data_db->table("valreal")
            ->order("sensor_id")
            ->group("valname_id, sensor_id")
            ->select("DATETIME(MAX(timestamp), ?) AS timestamp, value, valname.name AS value_name, sensor.name AS sensor_name, valname.unit.name AS units_name, sensor_id", "localtime");
	*/
        /******** Log request *******/
        // since log db is separate and should be ordered by timestamp - this request should be (is) really fast
        $this->template->logs = $this->log_db->table("log")->select("message")->limit(200)->order("timestamp DESC");

        /******** Graph request support *******/
        $this->template->date_to = new DateTime();
        $this->template->date_from = $this->template->date_to->modifyClone('-1 day');
        $this->template->date_to = $this->template->date_to->format("d.m.Y H:i");
        $this->template->date_from = $this->template->date_from->format("d.m.Y H:i");
        // $this->template->scripts = $this->script_db->table("script");
    }

    public function handleExe() {
        //bdump($httpRequest = $this->getHttpRequest());
        $this->template->scripts = $this->script_db->table("script");
        if (!$this->isAjax())
            $this->redirect('this');
        else
            $this->redrawControl('Exe');
    }

    public function handleDelscr($delete) {
        if (isset($delete) && $this->script_db->table("script")->where('name', $delete)->count())
        {
            $this->script_db->table("script")->where("name", $delete)->delete();
            $this->template->scripts = $this->script_db->table("script");
            if (!$this->isAjax())
                $this->redirect('this');
            else
                $this->redrawControl('Exe');
        }
    }

    protected function createComponentScriptForm()
    {
        $form = new Form;
        $form->setTranslator(new MyTranslator());
        $form->getElementPrototype()->class('ajax');
        $form->addText('name', 'Script name')->setRequired();
        $area = $form->addTextArea('script', 'Script content:')->setRequired()->setHtmlAttribute("rows", 20)->setHtmlAttribute("cols", 120);
        $form->addSubmit('send', 'Add Script');
        $form->onSuccess[] = [$this, 'ScriptFormSucceeded'];
        return $form;
    }

    public function ScriptFormSucceeded(Nette\Application\UI\Form $form, $values)
    {
        if($this->script_db->table("script")->where('name', $values["name"])->count())
            $this->script_db->table("script")->where('name', $values["name"])->update(['script' => $values["script"]]);
        else
            $this->script_db->table("script")->insert(['name' => $values["name"], 'script' => $values["script"]]);

        $this->template->scripts = $this->script_db->table("script");

        if (!$this->isAjax())
            $this->redirect('this');
        else
            $this->redrawControl('Exe');
    }

    public function handleGraph($sensors = [], $value, $date_from, $date_to) {
        $this->GraphUpdate($sensors, $value, $date_from, $date_to);
    }

    private function GraphUpdate($sensors = [], $value, $date_from, $date_to) {
        if (isset($sensors) && !empty($sensors) && isset($date_from) && isset($date_to) && isset($value)) {
            $request = $this->data_db->table("valreal")
                        ->select('STRFTIME("%s",timestamp) AS local_timestamp, value, sensor.name AS sensor_name')
                        ->order('timestamp')
                        ->where('sensor_id', array_unique($sensors))
                        ->where('valname.name', $value)
                        ->where('timestamp > DATETIME(?,?,?)', DateTime::from($date_from)->getTimestamp(), 'unixepoch', 'localtime')
                        ->where('timestamp < DATETIME(?,?,?)', DateTime::from($date_to)->getTimestamp(), 'unixepoch', 'localtime');
            $this->template->sensors = $this->data_db->table("sensor")->where("id", array_unique($sensors));
            $this->template->units = $this->payload->units = $this->data_db->table('unit')
                                        ->select('unit.name')
                                        ->where(':valname.name', $value)
                                        ->fetch()['name'];
            $this->payload->value_type = $value;
            $values = [];
            $firstrow = [];
            foreach ($this->template->sensors as $row) {
                array_push ($firstrow, $row['name']);
            }
            foreach ($request as $row) {
                $nextrow = [(int)$row['local_timestamp']];
                foreach ($firstrow as $sensor) {
                    if ($sensor == $row['sensor_name'])
                        array_push ($nextrow, (double) $row['value']);
                    else
                        array_push ($nextrow, null);
                }
                array_push ($values, $nextrow);
            }
            $this->template->period = [$date_from, $date_to];
            $this->payload->firstrow = $firstrow;
            $this->payload->values = $values;
            $this->template->stats = [];
            foreach ( $sensors as $sensor )
                $this->template->stats[$sensor] = $this->data_db->table("valreal")
                        ->select("AVG(value) AS average, (MAX(value) - MIN(value)) AS diff, sensor.name AS sensor_name")
                        ->where('sensor_id', $sensor)
                        ->where('valname.name', $value)
                        ->where('timestamp > DATETIME(?,?,?)', DateTime::from($date_from)->getTimestamp(), 'unixepoch', 'localtime')
                        ->where('timestamp < DATETIME(?,?,?)', DateTime::from($date_to)->getTimestamp(), 'unixepoch', 'localtime');
        }
        if (!$this->isAjax())
            $this->redirect('this');
        else
            $this->redrawControl('Graph');
    }

    protected function createComponentGraphForm()
    {
        $httpRequest = $this->getHttpRequest();
        $sensors = 0;
        if (null !== $httpRequest->getPost("sensors"))
            $sensors = $httpRequest->getPost("sensors");
        else
            $sensors = $httpRequest->getQuery("sensors");

        $value = 0;
        if (null !== $httpRequest->getPost("value"))
            $value = $httpRequest->getPost("value");
        else
            $value = $httpRequest->getQuery("value");

        $date_from = 0;
        if (null !== $httpRequest->getPost("date_from"))
            $date_from = $httpRequest->getPost("date_from");
        else
            $date_from = $httpRequest->getQuery("date_from");

        $date_to = 0;
        if (null !== $httpRequest->getPost("date_to"))
            $date_to = $httpRequest->getPost("date_to");
        else
            $date_to = $httpRequest->getQuery("date_to");

        $form = new Form;
        $form->setTranslator(new MyTranslator());
        $form->getElementPrototype()->class('ajax');
        $form->addDateTimePicker('date_from', 'Measurement from')
                  ->setFormat('m.d.Y H:i')
                  ->setRequired(TRUE)
                  ->setDefaultValue($date_from);
        $form->addDateTimePicker('date_to', 'Measurement to')
                  ->setFormat('m.d.Y H:i')
                  ->setRequired(TRUE)
                  ->setDefaultValue($date_to);
        $opt = $this->data_db->table("valsensor")
            ->where('valname.name', $value)
            ->select("sensor_id, sensor.name AS sensor_name")
            ->fetchPairs("sensor_id", "sensor_name");

        $checkboxlistg = $form->addCheckboxList('sensors', 'Sensors:', $opt);
        if (isset($sensors) && !empty($sensors))
            $checkboxlistg->setDefaultValue(array_unique($sensors));
        $form->addHidden('value', $value);
        $form->addSubmit('send', 'Update Graph');
        $form->addProtection('Time limit passed, please fill form and send again');
        $form->onSuccess[] = [$this, 'GraphFormSucceeded'];
        return $form;
    }

    public function GraphFormSucceeded(Nette\Application\UI\Form $form, $values)
    {
        $httpRequest = $this->getHttpRequest();  // ok I have to use this as kind of workaround because $values["sensors"] does not work but $httpRequest->getPost("sensors") surprisingly does.
        $this->GraphUpdate($httpRequest->getPost("sensors"), $httpRequest->getPost("value"), $httpRequest->getPost("date_from"), $httpRequest->getPost("date_to"));
    }
}
