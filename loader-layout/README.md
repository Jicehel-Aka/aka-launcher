# Disposition loader / game (nouveau loader)

Le dossier racine garde la disposition OTA (app0 / app1). Pour compiler le Launcher pour
la disposition **loader (test @0x10000) + game (factory @0x100000)** avec le bootloader
modifie (choix selon la cause du reset) :

1. copier `loader-layout/partitions.csv` a la racine (remplace `partitions.csv`) ;
2. copier `loader-layout/bootloader_components/` a la racine ;
3. `idf.py fullclean build` puis flasher le binaire dans `game` (0x100000) : au premier
   demarrage le Launcher s'auto-installe dans `loader`.

Un meme binaire detecte la disposition a l'execution (par type/sous-type de partition).
