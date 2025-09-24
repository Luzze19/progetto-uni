Sistema di Gestione Emergenze
Questo progetto implementa un sistema di gestione delle emergenze tramite l'uso di code di messaggi POSIX, thread e mutex per simulare un ambiente real-world. La comunicazione avviene tra un Server (l'eseguibile emergenza) e un Client (l'eseguibile client).

Il Server si mette in ascolto su una coda di messaggi per ricevere richieste di emergenza. Per ogni richiesta, crea un nuovo thread per la gestione dell'emergenza. Il sistema gestisce le emergenze in base a un sistema di priorità (da 0 a 2), dove quelle con priorità più alta vengono gestite per prime. Se non ci sono soccorritori disponibili, l'emergenza viene messa in coda in base alla sua priorità.

Il Client può inviare richieste di emergenza al Server, sia in modo interattivo da riga di comando che leggendo le richieste da un file.
