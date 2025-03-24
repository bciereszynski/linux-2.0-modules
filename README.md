# Kompilacja

### Wymagania
- Jądro musi być conajmniej poprawnie skonfigurowane.
```zsh
make menuconfig
```
- module.c nie musi być umieszczony w drzewie źródłowym (może być np. w /root)

### Kompilacja z użyciem skryptu
```zsh
./gcc-module.sh module.c
```

Powinien powstać plik wynikowy **module.o**



# Zarzadzanie modułem

### Załadowanie modułu
Aby załadować moduł użyj polecenia:

```zsh
insmod module
```

### Wyświetlenie listy modułów
Aby zobaczyć listę wszystkich załadowanych modułów wraz z licznikami odwołań, użyj:

```zsh
lsmod
```

### Usunięcie modułu
Aby usunąć moduł wykonaj:

```zsh
rmmod module
```