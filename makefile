# Nome dell'eseguibile
TARGET = emergenza

# Compilatore
CC = gcc

# Flag di compilazione
CFLAGS = -Wall -Wextra  -pthread #wall w wextra servono per attivare tutti i tipi di errore

# File sorgenti
SRCS = main.c parse_env.c parse_rescuers.c parse_emergency_types.c log.c

# File oggetto
OBJS = $(SRCS:.c=.o) #converte file c in o per la compilazione

# Librerie necessarie, linka la libreria librt (necessaria per mq_* e timer POSIX).
LIBS = -lrt

# Regola principale
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Regole implicite, MMD Mp dicono al compilatore di generare dei file.d con le dipendenze dei file.h
%.o: %.c 
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@ 

# Include le dipendenze generate
-include $(OBJS:.o=.d) #include i file .d nel mkefile così se modifichiamo un .h ricompiliamo tutto cio che li include

# Pulizia dei file oggetto e dell'eseguibile
clean:
	rm -f $(OBJS) $(TARGET)

# Pulizia profonda (inclusi file temporanei)
distclean: clean
	rm -f *~ *.log

# Esecuzione del programma
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean distclean run
