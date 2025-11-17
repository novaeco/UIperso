# Dépannage compilation ESP-IDF / Windows

## Échecs de configuration CMake après mise à jour d’ESP-IDF

Lors d’une mise à jour ou d’un nettoyage partiel, certains binaires ou dépendances peuvent manquer.
## Erreur `argtable3.h` manquant durant la configuration

Symptôme observé sur Windows :

```
CMake Error at .../components/argtable3/CMakeLists.txt:7 (file):
  file COPY cannot find ".../components/argtable3/argtable3/src/argtable3.h"
```

La cause est presque toujours un environnement ESP-IDF incomplet (sous-modules non initialisés ou installation partielle via `idf-env`).

### Vérification rapide

```powershell
cd UIperso
python tools/validate_idf_env.py
```

Le script contrôle que `IDF_PATH` est défini, pointe vers une arborescence valide et indique la version détectée.
Le script vérifie `IDF_PATH` et la présence du header argtable3.

### Remédiation recommandée

1. **S’assurer que l’environnement est chargé** (`export.bat`/`export.ps1` ou `idf.py --version`).
2. **Récupérer les sous-modules et dépendances** :
2. **Récupérer les sous-modules manquants** :
   ```powershell
   git -C %IDF_PATH% submodule update --init --recursive
   ```
3. **Nettoyer et relancer la configuration** :
   ```powershell
   idf.py fullclean
   idf.py set-target esp32s3
   ```
4. Si l’erreur persiste, lancer `idf.py doctor` puis réinstaller ESP-IDF via `idf-env.exe install` (corrige les installations corrompues ou partielles).
4. Si l’erreur persiste, lancer `idf.py doctor` puis réinstaller ESP-IDF via `idf-env.exe install` (corrige les installations corrompues).

Après correction, relancer `idf.py build`.
