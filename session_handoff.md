# Session handoff — DQIX Enhanced Linux UI (direction SDL2)

**Dernière mise à jour :** 2026-10-29  
**Dépôt :** https://github.com/rakmahefa/dqix-enhanced  
**Branche :** plan/v32-linux-launcher-settings  
**Base de travail :** main à ac8c5ae5c9f9b30e17b7ae379c6fd6413891949a.  
**Commit parent de cette révision documentaire :** 8159f568972d65215e8ba64363dca5e77b12a600.

## 1. Décision utilisateur à suivre

- **SDL2 comme socle commun** du launcher Linux, du runtime existant et des nouvelles interfaces.
- **Aucune nouvelle bibliothèque UI** : ne pas ajouter Dear ImGui.
- **Pas de Vulkan dans cette phase** ; réévaluer plus tard uniquement si une nécessité démontrée l’exige.
- UI pré-Play basée sur SDL2 + SDL_Renderer et une couche minimale de widgets propre au projet.
- Partager modèle de paramètres, actions et logique des pages ; ne pas supposer que le launcher et le menu in-game partagent fenêtre ou cycle de vie.
- Ne modifier aucune source Windows existante ni sa configuration de build. Les nouveaux sources natifs Linux restent dans third_party/extra/linux/.

Le plan de référence est plan-spec.md. Tout ancien code ou commentaire SDL3/ImGui/Vulkan est un prototype historique, pas l’architecture cible.

## 2. État réel du dépôt

Cette révision est **documentaire uniquement** ; le prototype et la CI n’ont pas encore été migrés :

- third_party/extra/linux/CMakeLists.txt requiert encore SDL3, Vulkan et Dear ImGui via FetchContent.
- third_party/extra/linux/ui/launcher_main.cpp reste le prototype SDL3/Vulkan/ImGui.
- third_party/extra/linux/vendor/imgui/LICENSE.txt et third_party/extra/linux/tests/smoke-ui.sh appartiennent encore à ce prototype historique.
- .github/workflows/linux-dev-sandbox.yml installe toujours SDL3/Vulkan, exécute vulkaninfo et a des inputs Vulkan. Il doit être adapté avec le prototype.
- third_party/extra/linux/settings/settings_model.cpp reste une couche de données utile, indépendante de SDL/UI. Son test de parité est ajouté mais le dernier résultat CI n’est pas confirmé.
- Le prototype historique n’est pas intégré à l’AppImage ni au bouton Play du builder Tk.
- Aucun statut de CI ne doit être décrit comme réussi sans consulter le vrai run et ses logs/artifacts.

**Conséquence :** ne pas simplement valider ou empaqueter le launcher SDL3 actuel. Convertir la cible et sa CI vers SDL2, puis compiler et tester la vraie cible.

## 3. Règles de protection

1. Ne modifier aucune source Windows existante ni sa configuration de build.
2. Ne pas modifier build.mk ni tools/build-game.sh : ces fichiers sont partagés par Windows et Linux.
3. Ne pas ajouter de shim/symlink source sous runtime/.
4. Placer les nouveaux sources C/C++/headers/tests natifs Linux sous third_party/extra/linux/.
5. Préserver les logs même si un test échoue ; un test ignoré n’est pas un succès.
6. Ne pas inclure de ROM, de sauvegarde ou d’asset protégé dans les artefacts CI/AppImage.
7. Distinguer préférence de rendu demandée, renderer gameplay effectif et renderer UI.

## 4. Constats runtime audités

- runtime/frontend.h expose frontend_init(), frontend_frame() et frontend_shutdown().
- runtime/frontend_sdl.c est le frontend SDL2 du jeu : fenêtres, événements, rythme des frames, présentation et état de pause. Il utilise OpenGL quand disponible et conserve un fallback SDL_Renderer/logiciel.
- runtime/controls.c mappe F5 vers le changement de mode. Le chemin existant dépend d’une capacité OpenGL ; reproduire le problème avec logs plutôt que de supposer une cause unique.
- runtime/menu_win32.c et plusieurs dialogues sont Windows-only et doivent rester inchangés.
- build.mk sélectionne le frontend par un chemin codé sous runtime/. Aucun point d’injection actuel ne sélectionne un frontend dans third_party/extra/linux/.
- tools/build-game.sh sert aux builds Windows et Linux. Une entrée de build Linux isolée reste à concevoir pour les éventuels changements du frontend.
- packaging/linux/gui.py est la fenêtre Tk de build ; sa redirection de stdout/stderr peut cacher des erreurs runtime.
- settings.ini dépend actuellement du répertoire de travail et l’état mémoire peut être réécrit à l’arrêt.

## 5. Prochaines étapes

1. **P0 :** mettre à jour la CI de développement pour installer SDL2, enlever les étapes Vulkan et conserver les logs d’installation/configuration/build/tests.
2. **P0 :** remplacer le prototype SDL3/ImGui/Vulkan par une petite app SDL2 + SDL_Renderer, sans nouvelle bibliothèque UI. Maintenir l’aide et les tests sans ROM.
3. **P1 :** vérifier le modèle de paramètres et les tests de parité ; stabiliser chemin, validation, sauvegarde atomique et diagnostics.
4. **P2 :** construire le launcher SDL2, puis tester édition de paramètres, lancement simulé, logs et fermeture. Raccorder au bouton Play/Tk et à l’AppImage uniquement après des tests réels.
5. **P3 bloqué :** démontrer une intégration in-game isolée Linux qui partage événements et cycle de rendu/pause, avec OpenGL et un comportement défini pour le fallback. SDL2 commun ne suffit pas à lui seul.
6. Contrôler le diff Windows à chaque étape. Ne déclarer aucune fonctionnalité sans effet validé dans le runtime.

## 6. CI et validation

La CI temporaire existe pour permettre scripts, tests ciblés, diagnostics et collecte de logs sans construire toute l’AppImage à chaque itération. Elle reste actuellement configurée pour SDL3/Vulkan et est donc **non alignée** avec la nouvelle direction.

Pour la suite :
- déclenchement limité à la branche de développement et lancement manuel ;
- SDL2 et toolchain C/C++ installés explicitement ;
- build Linux reproductible via CMake/Ninja ou autre orchestration isolée ;
- tests modèle/settings sans ROM ni fenêtre ;
- smoke test SDL2 via Xvfb si pertinent ;
- artefacts de logs uploadés même en cas d’échec ;
- test complet AppImage ponctuel avant intégration finale, pas à chaque petite modification.

## 7. Conditions de validation finale

- [ ] Aucun source/configuration de build Windows protégé modifié.
- [ ] Aucun nouveau source natif Linux hors de third_party/extra/linux/.
- [ ] Launcher cible construit avec SDL2, sans ImGui/SDL3/Vulkan.
- [ ] Tests de paramètres exécutés et résultats consultés.
- [ ] UI teste navigation, actions, focus et erreurs en renderer logiciel.
- [ ] Chemins/settings/logs explicites et paramètres non perdus silencieusement.
- [ ] Menu in-game validé sur le vrai runtime avant toute annonce de disponibilité.
- [ ] Intégration builder/AppImage testée ; artefacts sans ROM/save/assets protégés.
- [ ] Diff Windows et chemin upstream revus.
