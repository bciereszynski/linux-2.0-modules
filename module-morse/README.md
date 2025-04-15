# PROJEKT - Nadajnik alfabetu Morse'a
Sterownik urządzenia znakowego tylko do zapisu. 

## Wymagania
- W wyniku  operacji write znaki odpowiadające dużymi i małym literom ASCII, spacji (oznacza pauzę) oraz cyfrom są nadawane za pomocą alfabetu Morse'a. Pozostałe znaki są ignorowane.
- Sygnały świetlne są nadawane poprzez zmianę koloru maksymalnie lewego­górnego znaku na ekranie.
- ~~Powrót z funkcji write następuje po wysłaniu wszystkich znaków.~~
- Zapewniona jest synchronizacja na wypadek, gdy kilka procesów usiłuje jednocześnie wykonać operację write. 
- Czas trwania kropki, kreski oraz pauzy może byc zmieniany przy pomocy ioctl. 
- Sterownik obsługuje osiem urządzeń o różnych numerach podrzędnych. 
- ~~Dopuszczalne jest ciągłe oczekiwanie w pętli przy wysyłaniu znaków.~~

### Dodatkowo:
1. (10p) Sterownik kompilowany jest jako moduł.
2. (20p): operacja write jest buforowana
   - Wielkość bufora urządzenia (początkowo 256 bajtów) można zmieniać za pomoca operacji ioctl w zakresie od 0 do 1024 bajtów
   -  Zmiana rozmiaru bufora nie może powodować utraty danych. 
   -  Powrót z write następuje po wstawieniu znaków do bufora, zamknięcie urządzenia nie może anulować transmisji znaków będących aktualnie w buforze. 
   - Sterownik nie może ciągle oczekiwać w pętli.