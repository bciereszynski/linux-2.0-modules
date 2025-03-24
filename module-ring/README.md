# ZADANIE - Modyfikacja sterownika ring

## Wymagania

Zmodyfikuj kod sterownika **ring** tak aby spełniał następujące wymagania:

1. **Obsługa wielu buforów**  
   - Sterownik powinien obsługiwać **cztery niezależne bufory pierścieniowe**.  
   - Bufory powinny być rozróżniane za pomocą **numeru podrzędnego urządzenia**.

2. **Obsługa polecenia `ioctl` do zmiany długości bufora**  
   - Użytkownik powinien móc dynamicznie zmieniać długość bufora.  
   - Długość bufora powinna mieścić się w zakresie **[256B - 16KB]**.  
   - Podanie wartości spoza tego zakresu powinno być traktowane jako **błąd**.  
   - Zmiana długości bufora powinna być możliwa **nawet wtedy, gdy znajdują się w nim jakieś znaki**.  
   - **Synchronizacja**: `kmalloc` może spowodować uśpienie procesu i przełączenie kontekstu – należy to uwzględnić.

3. **Obsługa polecenia `ioctl` do odczytu długości bufora**  
   - Użytkownik powinien mieć możliwość **odczytu aktualnej długości bufora**.

4. **Moduł jądra** ✅
   - Sterownik powinien być możliwy do skompilowania jako **moduł jądra

