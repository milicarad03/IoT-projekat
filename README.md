# IoT projekat
Projekat iz predmeta Bežične mreže
Sistem za nadzor mostova je IoT sistem za rano otkrivanje strukturnih anomalija i hitna obaveštenja 
na mostovima. Sistem kontinuirano prati napone, vibracije, nagibe, temperature i pomeranja, a 
zatim detektuje događaje poput potencijalnog oštećenja nakon zemljotresa, zamora konstrukcije, 
pojava pukotina ili udara vozila/broda u stubove. Sistem prikuplja podatke sa senzorskih čvorova, 
analizira ih lokalno i u oblaku i obaveštava nadležne službe u realnom vremenu. Na ovaj način se na 
vreme može detektovati potencijalni rizik za bezbednost mosta i omogućiti brza reakcija, čime se 
smanjuje mogućnost havarija ili oštećenja. 
Softverska simulacija omogućava testiranje i demonstraciju rada sistema u kontrolisanim uslovima, 
bez potrebe za fizičkom opremom, što znatno smanjuje troškove i rizik tokom faze razvoja. Osim 
toga, ovakav pristup omogućava detaljno praćenje i analizu podataka, kao i optimizaciju algoritama 
za detekciju anomalija. 
Jedan od ključnih aspekata projekta je upotreba MQTT protokola za komunikaciju između senzora, 
kontrolnog modula i aktuatora. Podaci o stanju mosta se u realnom vremenu prenose do centralnog 
kontrolnog sistema, koji na osnovu unapred definisanih pravila može aktivirati različite reakcije 
uključujući vizuelne ili zvučne alarme, automatsko slanje obaveštenja nadležnim službama ili 
pokretanje preventivnih mehanizama za zaštitu konstrukcije. 
Ovaj projekat demonstrira kako IoT tehnologije mogu doprineti značajnom unapređenju 
bezbednosti infrastrukturnih objekata. Takođe pokazuje da je moguće razviti efikasne i pouzdane 
sisteme za nadzor mostova koji su primenljivi u realnim uslovima, što predstavlja korak ka 
modernizaciji i digitalizaciji infrastrukture.
• Senzori: mere temperaturu, naprezanje, vibracije i ultrazvučno rastojanje sa simulacijom 
slučajnih podataka za testiranje. Svaki senzor (senzorA, senzorB, senzorC) ima specifičan 
skup merenja: senzorA prati temperaturu, senzorB vibracije i ultrazvuk, dok senzorC 
pokriva sve četiri vrste podataka. Simulacija se temelji na slučajnim vrednostima unutar 
realističnih raspona (npr. temperatura 20-70°C, naprezanje 0-0.008) kako bi se testirala 
funkcionalnost sistema u kontrolisanim uslovima. 
• Bridge kontroler : Implementira SSDP (Simple Service Discovery Protocol) za otkrivanje 
uređaja u mreži, omogućavajući automatsko prepoznavanje senzora i drugih komponenti. 
Takođe koristi MQTT protokol za razmjenu podataka, pružajući pouzdanu i real-time 
komunikaciju. Podržava komande poput start (pokretanje senzora), stop (zaustavljanje) i  
set_period (podešavanje perioda izvođenja u sekundama), koje se šalju putem specifičnih 
tema (npr. bridges/mostA/commands/{device\_id}). 
• Aktuator : Odgovorni su za praćenje unapred definiranih pragova kako bi detektovali 
potencijalne probleme na mostu. Praćenje uključuje temperature iznad 60°C, naprezanje 
veće od 0.005, vibracije preko 2.5 i ultrazvučno rastojanje ispod 2.0 metara, što može 
ukazivati na pukotine. Kada se pragovi premaše, aktuatori ispisuju upozorenja u konzoli i 
simuliraju aktivnosti poput slanja alarma, omogućavajući brzu reakciju na kritične situacije. 

MQTT je publish/subscribe protokol idealan za IoT simulacije koji omogućava ekifasnu 
komuniikaciju između uređaja.  
Lokalni Mosquitto broker služi kao centralno čvorište za distribuciju poruka. 
Simulirani senzori objavljuju podatke na određene teme.  
Lokalni kontroler prikuplja podatke, pretplaćuje se na teme senzora i objavljuje stanje. 
Simulirani aktuatori izvršavaju akcije (npr. aktivaciju sirene). Pretplaćuju se na temu na koju 
kontroler objavljuje stanje. 
