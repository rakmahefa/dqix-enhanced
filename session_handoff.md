# Session handoff — DQIX Enhanced Linux launcher (SDL3 / ImGui / Vulkan)

**Date de rédaction :** 2026-10-28  
**Dépôt :** https://github.com/rakmahefa/dqix-enhanced  
**Branche de travail :** `plan/v32-linux-launcher-settings`  
**Branche de base :** `main`  
**Commit de base connu :** `ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a`  
**Dernier commit confirmé avant cette mise à jour de la note :** `d545274c965eac8a10a2e16c2368e51e7f4071eb`  
**État fonctionnel :** plan et CI de développement ajoutés ; le launcher et le menu ne sont pas encore implémentés.

## 1. Règles impératives

1. **Ne modifier aucun fichier source Windows existant ni sa configuration de build.** La compatibilité avec l'upstream Windows doit rester possible. Le menu Win32 et ses ressources doivent rester intacts.
2. Tout **nouveau fichier source natif Linux** (C/C++, headers, shaders, adaptateurs, glue code, tests natifs) doit être sous `third_party/extra/linux/`. Ne pas disperser les nouveaux sources dans `runtime/` ou ailleurs.
3. La nouvelle UI Linux utilise **SDL3 + Dear ImGui + Vulkan**.
4. **UI Vulkan ≠ renderer gameplay Vulkan.** Le jeu utilise actuellement un chemin SDL2/OpenGL avec fallback logiciel. Ne pas annoncer Enhanced/Vulkan gameplay simplement parce que le launcher Vulkan se lance.
5. Si l'intégration in-game nécessiterait de modifier un source partagé avec Windows, arrêter et chercher un point d'extension ou un adaptateur Linux isolé. Documenter le blocage plutôt que contourner la règle.

## 2. Documents et CI disponibles

- Plan de référence : [plan-spec.md](https://github.com/rakmahefa/dqix-enhanced/blob/plan/v32-linux-launcher-settings/plan-spec.md)
- CI temporaire : [linux-dev-sandbox.yml](https://github.com/rakmahefa/dqix-enhanced/blob/plan/v32-linux-launcher-settings/.github/workflows/linux-dev-sandbox.yml)
- Ce document : [session_handoff.md](https://github.com/rakmahefa/dqix-enhanced/blob/plan/v32-linux-launcher-settings/session_handoff.md)

La CI temporaire est destinée aux itérations de développement. Elle est limitée à la branche de travail et peut également être lancée manuellement. Elle :
- installe les dépendances de diagnostic SDL3/Vulkan sur Ubuntu 24.04 (dont rsync pour le test de politique AppImage facultatif) ;
- enregistre les versions des outils et le résultat de `vulkaninfo` ;
- vérifie la frontière de sources Windows et le placement des nouveaux sources natifs ;
- configure/compile/teste `third_party/extra/linux/` s'il contient un `CMakeLists.txt` ;
- conserve les logs comme artefact `linux-dev-diagnostics`, avec rétention de 14 jours, même quand un job échoue ;
- expose les options manuelles `require_linux_prototype`, `require_vulkan_device` et `run_project_tests`.

Valeurs par défaut du lancement manuel : les exigences strictes et les tests généraux sont désactivés, pour permettre un run de diagnostic au tout début. Quand le prototype existe, activer `require_linux_prototype`; pour une validation d'exécution Vulkan, activer aussi `require_vulkan_device`. Si l'hôte CI n'expose pas de device, un avertissement/diagnostic n'est pas une preuve de rendu Vulkan.

La CI existante [linux-ci-appimage.yml](https://github.com/rakmahefa/dqix-enhanced/blob/main/.github/workflows/linux-ci-appimage.yml) n'a pas été modifiée. Elle vise `main`/`stable` et le lancement manuel ; elle construit l'AppImage et exécute les tests généraux. Ne pas imposer cette construction lourde à chaque petite expérience. Avant une PR finale qui affecte le build, les dépendances ou le packaging, exécuter la validation complète pertinente et ne pas déclarer un test passé s'il n'a pas été exécuté.

## 3. Constats techniques de l'audit précédent

- `runtime/controls.c` mappe F5 vers `CA_MODE`, mais l'action existante dépend de `g_gl`.
- `runtime/frontend_sdl.c` demande OpenGL 3.3. Si le contexte échoue, le runtime peut utiliser un renderer logiciel et forcer Classic comme état effectif ; la sauvegarde de fin de processus peut écraser la préférence Enhanced. La cause exacte sur la machine de test reste à confirmer par des logs.
- Le menu et plusieurs dialogues sous `runtime/menu_win32.c` sont Windows-only ; Linux n'a pas d'UI équivalente pour l'ensemble des options v32.
- `settings.ini` est ouvert par chemin relatif au répertoire courant et la sauvegarde peut réécrire l'état conservé en mémoire.
- `packaging/linux/gui.py` masque actuellement stdout/stderr du jeu en les redirigeant vers `DEVNULL`. Ajouter des diagnostics accessibles sans exposer les données privées de jeu.
- Let's Go Recomp sert d'inspiration pour les pages, la navigation et le modèle partagé des paramètres ; ne pas supposer que son code UI peut être copié directement.

## 4. Prochaine tâche recommandée

Commencer par **P0 — prouver les frontières d'intégration**, pas par écrire toute l'interface :

1. Ouvrir la CI temporaire et vérifier le run de branche ; télécharger l'artefact de logs. Si le workflow n'a pas été exécuté automatiquement, lancer manuellement `Linux development diagnostics (temporary)` sur `plan/v32-linux-launcher-settings`.
2. Examiner `platform-and-dependencies.txt`, `vulkaninfo-summary.txt`, `changed-files.txt`, `source-boundary-check.txt` et le résultat du probe Vulkan.
3. Cartographier les APIs existantes et l'ordre de vie du runtime : création fenêtre/contextes SDL2, dispatch clavier, pause/tick, settings, fermeture. Ne toucher à aucune source Windows pendant cette exploration.
4. Construire un prototype minimal exclusivement sous `third_party/extra/linux/` pour confirmer SDL3 + Vulkan + Dear ImGui avec la toolchain cible. Ajouter un `CMakeLists.txt` quand ce prototype est prêt ; les prochains runs de CI tenteront alors automatiquement la compilation et CTest.
5. Tester explicitement la faisabilité du menu in-game Escape, de la capture d'événements, de la pause/reprise et du fullscreen. Un launcher SDL3 autonome ne peut pas se dessiner automatiquement dans une fenêtre SDL2/OpenGL d'un autre processus ; Wayland ne fournit pas de hook clavier global générique. Il faut prouver le point d'intégration exact.
6. Si l'overlay exige de modifier un fichier partagé avec Windows, consigner le blocage et décider d'une adaptation/entrée Linux isolée avant P1/P2/P3. Ne pas prétendre que le menu Escape est faisable avant cette preuve.
7. Après P0, traiter la persistance des paramètres et les logs ; puis le launcher pré-Play ; ensuite seulement le menu in-game et la parité v32.

## 5. Critères de validation à ne pas oublier

- Diff contre `main` propre vis-à-vis des sources Windows.
- Tous les nouveaux sources natifs Linux sous `third_party/extra/linux/`.
- Logs conservés même en cas d'échec, avec version des outils, résultat Vulkan et sorties de build/test.
- Distinguer le renderer de l'UI du renderer gameplay ; ne pas confondre « Vulkan disponible » et « Enhanced actif ».
- Distinguer préférence demandée et capacité effective ; aucune modification silencieuse de préférence parce qu'une capacité manque.
- Pas de ROM, de sauvegarde ni d'assets protégés dans CI ou les artefacts.
- Ne déclarer comme réussis que les tests réellement exécutés ; toute limite d'environnement doit apparaître dans les notes de run.
