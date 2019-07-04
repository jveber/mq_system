<?php

namespace App\Presenters;

use Nette;
use Nette\Application\UI\Form;
use Nette\Utils\DateTime;

class ExePresenter extends Nette\Application\UI\Presenter
{
	/** @var Nette\Database\Context */
	private $database;
	private $log_db;

	public function __construct(Nette\Database\Context $script, Nette\Database\Context $log)
	{
		$this->database = $script;
		$this->log_db = $log;
	}

	public function renderDefault($name, $script, $delete)
	{
		 if (isset($delete) && $this->database->table("script")->where('name', $delete)->count())
		 {
			$this->database->table("script")->where("name", $delete)->delete();
		 }
		 $this->template->scripts = $this->database->table("script");
		 $this->template->logs = $this->log_db->table("log")->select("message")->limit(50)->order("timestamp DESC");
	}

	protected function createComponentGraphForm()
	{
		$form = new Form;
		$form->addText('name', 'Název skriptu')->setRequired();
		$area = $form->addTextArea('script', 'Obsah skriptu:')->setRequired()->setHtmlAttribute("rows", 20)->setHtmlAttribute("cols", 120);
		//$area->getControlPart()->setAttribute("cols", 300);
		$form->addSubmit('send', 'Přidat skript');
		$form->onSuccess[] = [$this, 'FormSucceeded'];
		return $form;
	}

	public function FormSucceeded(Nette\Application\UI\Form $form, $values)
    	{
		if($this->database->table("script")->where('name', $values["name"])->count())
			$this->database->table("script")->where('name', $values["name"])->update(['script' => $values["script"]]);
		else
			$this->database->table("script")->insert(['name' => $values["name"], 'script' => $values["script"]]);
	}
}
