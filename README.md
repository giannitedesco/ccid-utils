# ccid-utils - USB smartcard/RFID driver and development platform

Copyright (c) 2008 Gianni Tedesco

## INTRODUCTION

ccid-utils is a USB smartcard driver and development platform. The driver follows a simple synchronous design which supports multiple slots but only one transaction at a time and includes a python interface. It also includes a commandline smartcard shell with a searchable history. The shell, written in python, offers many useful features for developing with smart-cards as well as for reverse engineering APDU formats. The package also includes tools for reading data from GSM SIM cards and EMV credit/debit cards. The SIM tool is very basic but allows reading SMS messages from a SIM. An example EMV (credit/debit) card tool is included which is boilerplate code for utilizing the EMV C API. A graphical interface for reading EMV cards is also provided.

If you like and use this software then press [<img src="http://www.paypalobjects.com/en_US/i/btn/btn_donate_SM.gif">](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=gianni%40scaramanga%2eco%2euk&lc=GB&item_name=Gianni%20Tedesco&item_number=scaramanga&currency_code=GBP&bn=PP%2dDonationsBF%3abtn_donateCC_LG%2egif%3aNonHosted) to donate towards its development progress and email me to say what features you would like added.
