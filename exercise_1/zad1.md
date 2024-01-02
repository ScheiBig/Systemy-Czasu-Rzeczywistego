# Zadanie 1
Napisać program obsługujący sygnały czasu rzeczywistego zgodnie z następującymi założeniami:
1. program ma obsługiwać przynajmniej dwa różne sygnały;
2. dla obsługi każdego ze sygnałów ma być tworzony dedykowany wątek;
3. obsługa sygnałów ma być w wątkach synchroniczna, poprzez blokowanie wątku w 
oczekiwaniu na sygnał, nie dopuszczalne jest aktywne czekanie w wątku na sygnał, czyli w
nieskończonej pętli;
4. wątki powinny mieć prawidłowo ustawione maski sygnałów, tak aby obsługiwały jedynie 
sygnał, dla którego są dedykowane, pozostałe sygnały powinny blokować;
5. obsługa sygnałów ma obejmować odczytywanie przekazywanych z nimi danych;
6. pkt. 6 wymaga napisania dodatkowego programu, który będzie umożliwiał wysłanie sygnału z 
danymi (funkcja sigqueue), z konsoli tego zrobić się nie da;
7. program ma być zgodny ze standardem języka C i standardami POSIX.
