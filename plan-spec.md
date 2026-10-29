# Plan-spec — Linux v32 : launcher et menu in-game SDL2

**Statut :** décision d’architecture révisée le 2026-10-29. La cible est SDL2, sans nouvelle bibliothèque UI et sans Vulkan dans la première phase. Les prototypes déjà présents en SDL3/ImGui/Vulkan sont historiques et ne valident pas cette nouvelle direction.  
**Base de travail :** ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a sur main.  
**Branche :** plan/v32-linux-launcher-settings.

## 0. Règle primordiale : préserver l’upstream Windows

**Ne modifier ni les sources Windows existantes, ni leur configuration de compilation.** Cette contrainte est absolue.

- Ne pas modifier runtime/menu_win32.c, ses ressources, ses dialogues, les fichiers spécifiques à Windows, ou toute source/configuration partagée dont le comportement ou la compilation Windows serait affecté.
- Tous les nouveaux fichiers source natifs Linux (C, C++, headers, adaptateurs, tests spécifiques, code de rendu et glue code) restent sous **third_party/extra/linux/**.
- Les scripts et workflows exclusivement Linux peuvent rester dans leurs répertoires existants, mais ne doivent pas modifier le chemin de build Windows.
- **build.mk** et **tools/build-game.sh** sont partagés par les builds Windows et Linux : ne pas les changer dans le cadre de ce plan. Ne pas ajouter de shim ou symlink sous runtime/ pour contourner cette règle.
- L’intégration au runtime doit utiliser un point d’extension existant ou une chaîne de build Linux autonome et isolée. Si aucun raccordement propre n’existe, consigner le blocage plutôt que d’altérer les sources protégées.
- Avant une PR, auditer le diff et démontrer que les sources et le comportement du build Windows n’ont pas été touchés.

**Critère de rejet :** simplifier le port Linux au prix d’une modification des sources Windows ou de leur configuration de build est inacceptable.

## 1. Décision d’architecture

### Choix retenu

1. **SDL2 est le socle multimédia commun** au launcher Linux, au runtime existant et aux nouvelles interfaces Linux. Réutiliser au maximum la bibliothèque et les conventions déjà employées dans le dépôt et son packaging.
2. **Ne pas introduire Dear ImGui ni une autre bibliothèque de widgets.** Construire une couche UI SDL2 petite et limitée aux besoins du projet : panneaux, boutons, listes, sliders, cases à cocher, navigation clavier/manette, focus visible et dialogues simples.
3. **Ne pas ajouter Vulkan dans cette phase.** Le renderer gameplay existant utilise SDL2/OpenGL avec un chemin de repli SDL_Renderer/logiciel. Le launcher peut utiliser SDL2 + SDL_Renderer sans créer un second backend graphique.
4. **Pas de nouvelle dépendance de rendu ou de texte par défaut.** Réutiliser les ressources typographiques existantes lorsqu’elles conviennent ; sinon préférer une petite police bitmap dans le code Linux isolé. Ne pas ajouter SDL_ttf ou une autre bibliothèque sans besoin démontré et décision explicite.
5. **Séparer modèle, actions, widgets et rendu.** Les pages doivent appeler le même modèle typé de paramètres et les mêmes contrats d’actions. La logique de navigation et d’état UI ne doit pas être dupliquée pour chaque fenêtre ou renderer.
6. **Le launcher est d’abord un processus pré-Play autonome.** Le menu in-game est un chantier distinct : SDL2 commun ne signifie pas que les fenêtres, événements, contexte OpenGL, pause et cycle de vie du moteur sont déjà partagés.
7. **Vulkan reste une option future**, uniquement si une exigence concrète et mesurée ne peut être satisfaite raisonnablement avec les chemins présents. Toute proposition future devra avoir sa propre justification, preuve de faisabilité et analyse des dépendances.

### Conséquences pour le prototype existant

Le prototype présent sous third_party/extra/linux utilise encore SDL3 + Dear ImGui + Vulkan, et le workflow temporaire installe ces dépendances. Ils sont **hors de la nouvelle cible**. Cette révision ne change pas encore le code du prototype ; l’implémentation suivante devra le remplacer ou le réduire explicitement. Ne pas présenter son build/smoke test comme validation du launcher SDL2 cible.

La cible devra consommer SDL2 depuis l’environnement Linux existant, avec une détection reproductible via pkg-config ou les outils SDL2 déjà disponibles. Aucun FetchContent, vendoring ni téléchargement d’ImGui/Vulkan ne doit subsister dans le nouveau chemin.

## 2. Constats techniques déjà établis

| Zone | Constat | Conséquence |
|---|---|---|
| runtime/frontend.h | Expose frontend_init(), frontend_frame() et frontend_shutdown(). | Respecter le cycle de vie du frontend. |
| runtime/frontend_sdl.c | Le jeu utilise SDL2 ; gère fenêtres, événements, rythme des frames, pause et présentation. Le renderer OpenGL est optionnel. | Réutiliser le comportement existant, mais ne pas supposer qu’un processus launcher peut contrôler la boucle. |
| runtime/gl_render.c | Renderer gameplay OpenGL existant. | Le launcher ne doit pas remplacer ni réécrire le renderer gameplay. |
| runtime/controls.c | Définit actions, raccourcis et mappings clavier/manette ; F5 correspond au changement Classic/Enhanced. | Réutiliser les conventions d’actions ; vérifier les effets et les conflits de focus. |
| runtime/menu.h / runtime/menu_win32.c | Le menu et plusieurs dialogues dépendent de Win32. | Ne pas compiler ce menu comme UI Linux ni modifier son code. |
| build.mk | Sélectionne le frontend via un chemin sous runtime/ et ajoute les unités historiques en mode SDL. | Aucun point d’injection actuel ne choisit directement un frontend sous third_party/extra/linux/. Ne pas changer ce fichier partagé. |
| tools/build-game.sh | Sert aux builds Windows et Linux. | L’orchestration Linux dédiée doit être ailleurs. |
| packaging/linux/gui.py | Fenêtre Tk pour ROM/workspace/build et lancement ; stdout/stderr du jeu peuvent être redirigés vers DEVNULL. | Conserver le flux de build, mais rendre les logs accessibles. |
| runtime/frontend_sdl.c — settings | Les paramètres dépendent du répertoire courant et sont sauvegardés depuis l’état en mémoire. | Définir des chemins explicites et éviter l’écrasement silencieux. |

Les constats décrivent le code audité ; la cause exacte des symptômes F5/Enhanced sur une machine donnée reste à confirmer avec des logs d’une exécution reproductible.

## 3. Matrice fonctionnelle v32 à auditer

La présence d’une clé dans settings.ini ne prouve ni qu’elle est appliquée, ni que le renderer courant la prend en charge. Pour chaque option, mesurer la valeur demandée, la capacité requise, l’action, l’effet observé et la persistance.

| Famille | Éléments à vérifier | Exigence |
|---|---|---|
| Mode/renderer | Classic, Enhanced, résolution 3D/Auto | Distinguer préférence demandée, renderer effectif et limitation OpenGL ; aucun retour silencieux à Classic. |
| Qualité graphique | Textures lissées, outlines, shadows/glows, smooth shading | Ne montrer que les effets réellement appliqués par le chemin actif. |
| HD text | HD text, ombres/contours | Signaler les prérequis et l’effet réel. |
| Présentation des écrans | Stacked, side-by-side, corner, widescreen, two windows, HUD | Réutiliser les capacités du frontend ; identifier les options nécessitant un redémarrage. |
| Échelle/fenêtre | Taille, fullscreen, integer scaling, actual size, swap, single screen | Garder valeurs et focus cohérents avec les raccourcis existants. |
| Filtre DS Lite/console | Couleurs LCD, grille, ghosting, backlight, coque/skin, gap | Désactiver ou expliquer les effets indisponibles selon le renderer. |
| Mods texture | Packs, upscale, dump/reload, comparaison, chemins | Confirmer assets, opérations, progression et erreurs. |
| Mods musique | OGG/FLAC/WAV, reload, noms, volume, export | Réutiliser les services existants ; afficher les opérations longues et leurs résultats. |
| Langue/DLC | Langues EUR, DLC | Tenir compte de la région et du moment d’application. |
| Contrôles | Clavier, second binding, gamepad, commandes DS | Capture clavier/manette, focus, conflits et annulation par Escape. |
| Sauvegarde/session | Import/export, wireless, serveur, host/join, nom/port | Valider chemins et paramètres ; parcours Linux isolés pour remplacer les dialogues Win32. |
| Utilitaires | Pause, fast-forward, screenshots, rapports, update, Discord, volume | Réutiliser les actions existantes ; expliciter toute indisponibilité. |

## 4. Architecture cible SDL2

### 4.1 Launcher pré-Play

- Application Linux native distincte utilisant **SDL2 + SDL_Renderer** pour dessiner les contrôles et le texte. Pas d’ImGui, Vulkan, SDL3 ou SDL_ttf par défaut.
- Pages prévues : Game, Graphics, Screen & Display, Audio & Music, Controls, Mods, Wireless & Online et System. Les implémenter progressivement ; une page n’est pas réputée fonctionnelle tant que son action sous-jacente n’est pas testée.
- Page Game : chemin du jeu, build/fichiers requis, état des paramètres, lancement, statut du processus et accès aux logs/configurations.
- Le bouton Play ne s’active que lorsque l’exécutable et les données attendues existent. Le launcher lance le jeu avec un répertoire de travail défini et des arguments contrôlés ; erreurs visibles et journalisées.
- Une seule logique de paramètres et d’actions doit alimenter le launcher et le futur menu. Ne pas dupliquer les règles de validation dans les widgets.
- La fenêtre Tk reste celle de construction ROM/workspace au premier jalon ; elle pourra lancer le launcher SDL2 après le build.
- Aucun GPU Vulkan n’est requis. Tester le chemin SDL_Renderer logiciel en CI.

### 4.2 Couche UI minimale

- Composants de base : bouton, checkbox, slider, liste/choix, champ texte simple, barre latérale, groupe de paramètres, confirmation et bannière de diagnostic.
- Navigation clavier/manette avec ordre de tabulation stable, indicateur de focus visible, activation/annulation cohérente et capture de touche qui ne déclenche pas simultanément une action du jeu.
- États explicites : normal, focus, désactivé, en cours, succès et erreur. Une capacité absente doit être expliquée plutôt que cachée.
- Pas de travail lourd dans la boucle de rendu. Les opérations longues affichent une progression et ne doivent pas geler l’interface.
- Séparer le modèle et le layout des primitives SDL2. Les pages et la logique peuvent ainsi être partagées même si le launcher et l’overlay ont besoin de couches de dessin différentes.

### 4.3 Menu in-game : porte technique bloquante

Un processus SDL2 autonome ne peut pas dessiner dans la fenêtre du runtime, consommer sa file d’événements ni suspendre la boucle de jeu. Partager SDL2 ne suffit pas.

Avant de construire le menu, un prototype limité doit démontrer :

1. un point d’entrée Linux isolé permettant à l’UI de rejoindre les cycles d’événements et de rendu du jeu ;
2. l’ouverture/fermeture avec Escape sans déclencher une commande DS, ainsi que capture de touches et navigation manette ;
3. une pause/reprise vérifiable du tick du jeu ;
4. un dessin correct sur le chemin OpenGL et un comportement défini en fallback SDL_Renderer/logiciel ;
5. une fermeture propre et une gestion cohérente du fullscreen ;
6. aucune modification des sources ou de la configuration de build Windows.

Le rendu UI in-game peut nécessiter un adaptateur vers le contexte OpenGL existant et un chemin séparé pour le fallback. Ne pas inférer sa faisabilité à partir du launcher. Si le hook exige une modification partagée interdite, documenter le blocage et réévaluer P3 ; aucun shim ne doit être ajouté sous runtime/.

## 5. Contrats de paramètres et actions

### 5.1 Modèle de paramètres

- Continuer le modèle typé Linux sous third_party/extra/linux/settings/, indépendant de SDL2 et des widgets.
- Maintenir la parité avec les clés réellement lues/écrites par runtime/frontend_sdl.c.
- Préserver commentaires, ordre, clés inconnues et entrées non modifiées ; valider plages et formats des clés connues.
- Définir un chemin explicite pour settings.ini et controls.ini, indépendant du répertoire courant, avec migration sans perte et override de test.
- Le statut de lecture doit distinguer absent, chargé, partiellement invalide et erreur. Aucun fichier non valide ne doit être remplacé silencieusement.
- Écrire dans un temporaire du même répertoire, vérifier les erreurs, puis remplacer atomiquement la destination ; préserver les permissions si possible.
- Ne jamais enregistrer un état effectif temporaire comme nouvelle préférence quand une capacité manque.
- Le modèle ne doit importer ni SDL2 ni du code Windows et doit être testé sans ROM, fenêtre ou GPU.

### 5.2 Renderer et diagnostics

Le launcher et la page System affichent séparément :

- version SDL2 et renderer UI réellement utilisé ;
- renderer gameplay effectif (OpenGL ou fallback) et capacités pertinentes ;
- mode demandé et mode effectivement appliqué ;
- chemins settings, contrôles et logs ;
- résultats de lecture, validation, sauvegarde et lancement ;
- raisons des actions ou options indisponibles.

Les logs ne doivent contenir ni ROM, ni sauvegarde, ni contenu privé. Ne pas supprimer stdout/stderr sans conserver les erreurs pertinentes.

### 5.3 F5, Escape et actions

- F5 envoie une demande au contrat d’action du runtime et affiche le résultat effectif.
- Si Enhanced est impossible, expliquer la raison sans écraser la préférence demandée.
- Escape ne contrôle le menu in-game qu’après validation du bridge. En capture de touche, Escape annule la capture.
- Quand l’UI a le focus, ses événements ne déclenchent pas les commandes DS.
- Les actions longues (dump texture, export audio, rapport) affichent progression, succès ou erreur. Pas de faux succès.

## 6. Plan d’exécution

### CI temporaire de développement

Le workflow .github/workflows/linux-dev-sandbox.yml a été créé pour exécuter scripts/tests, conserver les diagnostics et éviter un build AppImage complet à chaque petite itération. **Il est actuellement configuré pour SDL3/Vulkan et doit être réaligné avant de valider le nouveau prototype.**

- Garder un workflow temporaire dédié à la branche de travail, lançable manuellement, sans AppImage complète par défaut.
- Installer SDL2 et les outils C/C++/CMake/Ninja/pkg-config ; enlever les contrôles Vulkan de la validation cible.
- Enregistrer OS, architecture, versions SDL2/compilateur, commandes de configuration/build/test et logs.
- Conserver les logs comme artefact même en cas d’échec ; un test ignoré n’est pas réussi.
- Vérifier la frontière Windows et l’emplacement de tous les nouveaux sources natifs Linux.
- Prévoir un profil sans GPU et sans ROM qui compile/teste le modèle et le launcher SDL2.
- Tests généraux et build AppImage complet restent optionnels pour les petites itérations, mais deviennent nécessaires avant une intégration finale pertinente.
- Réévaluer ou retirer la CI temporaire lorsque ses contrôles utiles sont fusionnés dans la CI permanente.

### P0 — Nettoyer la cible et prouver le socle

1. Confirmer les dépendances SDL2 déjà requises et les limites de la fenêtre Tk/AppImage.
2. Remplacer la cible SDL3/ImGui/Vulkan existante. Supprimer de la cible les exigences FetchContent, vendor ImGui et Vulkan.
3. Compiler un probe SDL2 qui ouvre une fenêtre, dessine via SDL_Renderer et se termine proprement. Tester aussi l’aide et le chemin sans fenêtre.
4. Tester la CI mise à jour et consulter ses logs ; ne pas inférer un succès sans run réel.
5. Définir le build Linux isolé sans modifier build.mk ni tools/build-game.sh.
6. Reproduire F5/Enhanced avec logs sur OpenGL et fallback.

**Sortie P0 :** probe SDL2 confirmé, CI ciblée fonctionnelle et procédure de build Linux isolée documentée.

### P1 — Paramètres fiables et renderer explicable

**État actuel :** un premier modèle de paramètres Linux et un test de parité existent. L’exécution CI n’est pas confirmée.

1. Stabiliser lecture, validation, chemins, sauvegarde atomique et migration.
2. Confirmer la parité entre le schéma et les clés lues/écrites par le runtime.
3. Tester fichier absent/non inscriptible, entrées invalides/inconnues, préservation et changements partiels.
4. Exposer capacités, renderer et différence entre préférence demandée et état effectif.
5. Garder le modèle autonome, sans dépendance SDL2.

**Sortie P1 :** paramètres fiables et diagnostic testable sans ROM ni fenêtre.

### P2 — Launcher SDL2 pré-Play

**État actuel :** ui/launcher_main.cpp reste un prototype SDL3/ImGui/Vulkan ; son build ne valide pas la cible SDL2.

1. Le remplacer par un launcher SDL2 + SDL_Renderer et la couche UI minimale.
2. Réutiliser le modèle de paramètres et les contrats d’action.
3. Ajouter les pages progressivement ; marquer comme indisponible toute action non reliée au runtime.
4. Tester l’exécutable, les arguments, le répertoire de travail et les logs de lancement.
5. Tester sans ROM, avec une arborescence simulée et sur un Linux utilisant le renderer logiciel.
6. Après validation seulement, raccorder le launcher au bouton Play de packaging/linux/gui.py et au packaging AppImage ; documenter les bibliothèques réellement résolues.
7. Garder la fenêtre Tk de build ROM/workspace.

**Sortie P2 :** launcher SDL2 compilé/testé, sans ImGui/Vulkan ni dépendance UI additionnelle.

### P3 — Menu in-game Escape

1. Ne commencer qu’après réussite de la porte technique en 4.3.
2. Résoudre l’intégration dans une entrée de build/frontend Linux isolée, sans modifier les sources Windows ou build.mk.
3. Partager modèle de paramètres, actions, navigation et états de contrôle avec le launcher.
4. Tester Escape, focus, pause/reprise et Quit sur les chemins graphiques pris en charge.
5. Parcourir la matrice v32 ; toute option sans effet réel doit avoir une raison et ne pas être présentée comme opérationnelle.

**Sortie P3 :** intégration in-game démontrée sur Linux ; sinon blocage et périmètre réduit documentés.

### P4 — Tests, documentation, packaging et upstream

1. Tests du modèle : parsing, limites, migration, permissions, atomicité et état demandé/effectif.
2. Tests navigation/actions : clavier, manette, focus, F5, Escape et annulation de capture.
3. Test SDL2 minimal et tests de fenêtre via Xvfb si l’environnement le prend en charge.
4. Vérifier AppImage, bibliothèques SDL2 résolues, logs, lancement après build et cache froid/chaud. Pas d’assets protégés dans les artefacts.
5. Garder la CI de développement légère ; lancer la validation complète avant PR ou modification de packaging.
6. Comparer avec main et vérifier les sources/configurations Windows protégées.
7. Mettre à jour docs/LINUX-APPIMAGE.md, README et le présent handoff.
8. Test manuel sous Linux avec ROM personnelle : premier lancement, paramètres, effets réellement supportés, sauvegarde, wireless, arrêt et redémarrage.

## 7. Critères d’acceptation

- [ ] Aucun fichier source Windows existant et aucune configuration de build Windows protégée n’est modifié.
- [ ] Tous les nouveaux fichiers sources natifs Linux sont sous third_party/extra/linux/.
- [ ] Le launcher cible compile avec SDL2 et n’introduit ni Dear ImGui, ni SDL3, ni Vulkan, ni autre bibliothèque UI.
- [ ] Le launcher utilise SDL_Renderer et fonctionne sans device Vulkan.
- [ ] La CI temporaire installe/teste SDL2, conserve les logs en cas d’échec et n’exécute pas le build AppImage complet par défaut.
- [ ] Le modèle de paramètres est testé indépendamment de l’UI et reste en parité avec les clés du runtime.
- [ ] Les logs distinguent renderer UI/gameplay et préférence/état appliqué.
- [ ] Les chemins settings/controls ne dépendent plus silencieusement du répertoire courant.
- [ ] Le bouton Play/packaging n’est raccordé qu’après les tests du launcher.
- [ ] Escape/in-game n’est annoncé que si un raccordement réel a été prouvé.
- [ ] Les fonctions non prises en charge sont désactivées avec explication ; aucun faux succès ou changement silencieux.
- [ ] Pas de ROM, sauvegarde ou asset protégé dans CI ou AppImage.
- [ ] Le diff final préserve l’upstream Windows et son chemin de build.

## 8. Hors périmètre du premier cycle

- Remplacer le renderer gameplay par Vulkan ou introduire Vulkan pour le launcher sans besoin démontré.
- Introduire Dear ImGui, SDL3, SDL_ttf ou une autre dépendance UI dans la cible initiale.
- Réécrire le runtime Windows ou modifier son build pour faciliter Linux.
- Promettre Enhanced ou des effets graphiques si le renderer actif ne les prend pas en charge.
- Remplacer la fenêtre Tk de build avant stabilisation du launcher.
- Considérer un launcher SDL2 comme preuve que le menu in-game est intégré.

## 9. Références

- Cycle frontend et settings : runtime/frontend.h, runtime/frontend_sdl.c.
- Rendu gameplay : runtime/gl_render.c.
- Actions clavier/manette : runtime/controls.c, runtime/controls.h.
- Menu Windows à préserver : runtime/menu_win32.c, runtime/menu.h.
- Build partagé à préserver : build.mk, tools/build-game.sh.
- Builder Linux : packaging/linux/gui.py.
- Matrice v32 : release-notes/v32.md.
- Inspirations UI et modèle de paramètres : https://github.com/rakmahefa/letsgo-recomp/blob/main/runtime/ui/launcher_lg.mm et https://github.com/rakmahefa/letsgo-recomp/blob/main/runtime/ui/settings_ui.mm.
