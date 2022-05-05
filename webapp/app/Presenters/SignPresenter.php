<?php

namespace App\Presenters;

use Nette;
use Nette\Application\UI\Form;

class SignPresenter extends Nette\Application\UI\Presenter
{
    public function signInFormSucceeded(Form $form, Nette\Utils\ArrayHash $values)
    {
      try {
        $this->getUser()->login($values->username, hash_pbkdf2("sha3-512", $values->password, "sůl", 1000));
        $this->redirect('Homepage:');

      } catch (Nette\Security\AuthenticationException $e) {
        $form->addError('Nesprávné přihlašovací jméno nebo heslo. - ' . hash_pbkdf2("sha3-512", $values->password, "sůl", 1000));
      }
    }

    protected function createComponentSignInForm()
    {
        $form = new Form;
        $form->addText('username', 'Uživatelské jméno:')
            ->setRequired('Prosím vyplňte své uživatelské jméno.');

        $form->addPassword('password', 'Heslo:')
            ->setRequired('Prosím vyplňte své heslo.');

        $form->addSubmit('send', 'Přihlásit');

        $form->onSuccess[] = [$this, 'signInFormSucceeded'];
        return $form;
    }
}
