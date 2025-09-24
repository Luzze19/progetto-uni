# Sistema di Gestione Emergenze

Questo progetto implementa un sistema di gestione delle emergenze tramite l'uso di **code di messaggi POSIX**, **thread** e **mutex** per simulare un ambiente real-world. La comunicazione avviene tra un **Server** (l'eseguibile emergenza) e un **Client** (l'eseguibile client).

Il **Server** si mette in ascolto su una coda di messaggi per ricevere richieste di emergenza. Per ogni richiesta, crea un nuovo thread per la gestione dell'emergenza. Il sistema gestisce le emergenze in base a un sistema di priorità (da 0 a 2), dove quelle con priorità più alta vengono gestite per prime. Se non ci sono soccorritori disponibili, l'emergenza viene messa in coda in base alla sua priorità.

Il **Client** può inviare richieste di emergenza al Server, sia in modo interattivo da riga di comando che leggendo le richieste da un file.

## Struttura dei file
### File sorgenti

* **main.c**: Contiene la logica principale del Server. Si occupa della creazione e gestione della coda di messaggi, dell'avvio dei thread per il trattamento delle emergenze e della gestione delle risorse concorrenti come mutex e condition variables.
* **client.c**: Implementa il Client che invia messaggi di emergenza al Server.
* **log.c / log.h**: Modulo per la gestione centralizzata del logging. Gli eventi del sistema, come l'apertura/chiusura dei file o il parsing delle configurazioni, vengono scritti su un file di log (**operazioni.log**) con accesso esclusivo garantito da un mutex.
* **parse_env.c / env.h**: Funzioni per il parsing del file di configurazione **env.conf** e per la creazione della "griglia" su cui si muovono i soccorritori. Le variabili globali come la larghezza, l'altezza e il nome della coda sono dichiarate in **env.h** e definite in **parse_env.c**.
* **parse_rescuers.c / rescuers.h**: Modulo che legge e interpreta il file di configurazione **rescuers.conf**. Crea un array di tipi di soccorritori (rescuer_type_t) e un array di "gemelli digitali" (rescuer_digital_twin_t) per rappresentare le singole unità disponibili nel sistema.
* **parse_emergency_types.c / emergency.h**: Gestisce il parsing del file **emergency_types.conf** per leggere e allocare i tipi di emergenza e i soccorritori richiesti per ciascuno di essi.
* **treat_emergency.h**: Definisce le strutture dati per le richieste di emergenza e lo stato di un'emergenza (emergency_t, emergency_status_t).
* **macro.h**: Contiene macro utili per la gestione degli errori, come il controllo dell'apertura/chiusura dei file, l'allocazione della memoria dinamica e le operazioni sulle code di messaggi.

### File di configurazione

* **env.conf**: Definisce le variabili di ambiente, come il nome della coda di messaggi e le dimensioni della griglia di gioco (width, height).
* **rescuers.conf**: Elenca i tipi di soccorritori disponibili, il numero di unità per tipo, la loro velocità e la loro base di partenza.
* **emergency_types.conf**: Specifica i tipi di emergenza riconosciuti dal sistema, la loro priorità e quali soccorritori sono richiesti per gestirle.
* **prova.txt**: Un file di esempio che può essere usato dal Client per inviare in sequenza più richieste di emergenza, dimostrando il funzionamento in modalità da file.
* **makefile**: File di build che automatizza la compilazione del progetto, la pulizia dei file e l'esecuzione.

## Come compilare ed eseguire
#### Prerequisiti
* Un compilatore C come **gcc**.
* La libreria **librt** per le code di messaggi POSIX (mq_*).

#### Compilazione
Per compilare il progetto, basta eseguire il **makefile** fornito:
```bash
Make
```

Questo creerà gli eseguibili **emergenza** (Server) e **client** (Client).

#### Esecuzione
Il progetto è composto da un Server e un Client che devono essere eseguiti in terminali separati.
1. #### avviare il Server
Apri un terminale e avvia il Server:
```bash
./emergenza
```
Il Server inizializzerà il sistema e si metterà in attesa di messaggi.

2. #### Eseguire il client
Apri un altro terminale. Il Client può essere utilizzato in due modi:
#### Modalità 1: Invio di una singola emergenza da riga di comando
```bash
./client <nome_emergenza> <coord_x> <coord_y> <delay_sec>
```
* **<nome_emergenza>**: Il tipo di emergenza (es. "incendio", "terremoto").
* **<coord_x>, <coord_y>**: Le coordinate dell'emergenza nella griglia.
* **<delay_sec>**: Un ritardo in secondi prima che il messaggio venga inviato.

#### Esempio:
```bash
./client incendio 150 100 5
```
#### Modalità 2: Invio di emergenze da un file
```bash
./client -f <file_emergenze.txt>
```
Il file di input deve contenere una lista di emergenze, una per riga, nel formato **nome_emergenza coord_x coord_y delay_sec**.
Nel progetto ho preparato un file **prova.txt** per testare questa modalità

## Monitoraggio dell'esecuzione
Per tenere traccia di tutte le operazioni del sistema, è possibile consultare il file di log chiamato **operazioni.log**. Questo file viene creato automaticamente nella stessa directory degli eseguibili e registra ogni evento rilevante, come il parsing dei file di configurazione, l'apertura e la chiusura delle risorse, e l'invio dei messaggi.

## Terminazione del programma
Per terminare il Server in modo corretto, è necessario interromperlo dal terminale in cui è in esecuzione.
1. Assicurati di essere nel terminale in cui hai avviato il comando **./emergenza**.
2. Premi la combinazione di tasti **Ctrl + C**.

Il programma rileverà il segnale di interruzione e avvierà la procedura di pulizia delle risorse (coda di messaggi, mutex, thread, memoria dinamica) prima di terminare in modo sicuro.






