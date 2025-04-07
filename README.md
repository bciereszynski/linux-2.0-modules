# Intellisens w vscode

Aby mieć intelisense w vscode trzeba stworzyć folder `./linux`

```zsh
mkdir linux
```

Następnie trzeba skopiować source kod linux'a
```zsh
cd linux
cp -r /mnt/guest/usr/src/linux/* .
```

c_cpp_properties.json musicie sami znaleźć swojego `stdarg.h`
```json
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "/usr/lib/gcc/x86_64-pc-linux-gnu/14.2.1/include"
            ],
            "defines": [
                "__KERNEL__"
            ],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "c17",
            "cppStandard": "gnu++17",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
```

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