# Message-management
 Implemented a client-server program(TCP and UDP) for message management

In realizarea acestei teme am respectat toate cerintele impuse,
 	resolvand in totalitate toata tema.

 	Atat clientul TCP, cat si serverul au fost implementati folosind
 	cunostintele acumulate la laborator, pentru lucrul cu socketi,
 	stabilire de conexiuni, trimitere si primire de mesaje.

 	CLIENTUL:
 	-dupa realizarea conexiuni trimite id-ul catre server, pentru
 	memorarea acestuia in vectorul de clienti al serverului
 	-poate primi 3 comenzi de la tastura : exit, subscribe topic sf si
 	unsubscribe topic, alt tip de comenzi fiind respinse de acesta,
 	raspunzand cu "Wrong command"
 	-primeste mesaj de la server atunci cand conexiunea este inchisa,
 	si poate primi de la clienti UDP mesaje de orice dimensiune

 	SERVERUL:
 	- se bazeaza pe un vector de clienti; adauga in vector fiecare
 	client nou conectat. In cazul in care un client s-a deconectat,
 	acesta ramane in vector, schimbandu-i-se doar campul connected.
 	- poate primi comanda exit care termina conexiunea
 	- deschide doi socketi(TCP si UDP); de la UDP primeste mesaje (topicuri)
 	pe care le manipuleaza conform enuntului si apoi le trimite
 	tuturor clientilor care sunt abonati la respectivul topic

 	- acesta se bazeaza pe clasa Client, care contine mai multe campuri:
 		- in cazul in care un client se deconecteaza, iar serverul primeste
 		mesaje de la clienti udp, topicurile la care acest client a fost
 		abonat se vor stoca intr-un vector(o cutie postala), iar atunci
 		cand acesta se va reconecta, va primi toate mesajele din cutia postala
 		- cat timp un client este conectat, acesta primeste mesaje de la
 		topicurile la care este abonat, doar daca clientii udp trimit mesaje
 		catre server in acel timp
 		- contine un vector cu toate topicurile la care este abonat, iar cand
 		acesta da comanda subscribe, topicul respectiv este adaugat in vector,
 		si eliminat in cazul in care se dezaboneaza


 	MODULARIZARE:
 		Tema am incercat pe cat posibil sa o impart in cat mai multe subfunctii
 		pentru o buna revizualizare si pentru o buna intelegere a ei. Aceasta
 		are multe comentarii pe cod, si sper ca acele comentarii combinate cu
 		acest readme sa dupa la o buna intelegere a ei.
