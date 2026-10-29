# Linux UI prototype — décision de migration SDL2

> **Architecture cible révisée le 2026-10-29 : SDL2, sans Dear ImGui et sans Vulkan dans la première phase.**  
> Le prototype décrit par une partie de ce répertoire a été commencé en SDL3 + Dear ImGui + Vulkan. Il est désormais historique et doit être remplacé avant d’être considéré comme base du launcher. Ce README documente l’écart entre le code présent et la cible décidée.

Référence d’architecture et plan : plan-spec.md à la racine du dépôt. Suivi de session : session_handoff.md.

## État actuel (code non encore migré)

- CMakeLists.txt requiert encore SDL3, Vulkan et Dear ImGui via FetchContent.
- ui/launcher_main.cpp est encore le prototype SDL3/Vulkan/ImGui.
- Le modèle dans settings/ est indépendant de SDL/UI et demeure réutilisable.
- Le prototype n’est pas le launcher SDL2 visé, n’est pas intégré au bouton Play du builder Tk et n’est pas empaqueté comme launcher dans l’AppImage publiée.
- Les résultats de compilation/CTest ne sont pas confirmés comme réussis dans cette note.

Les commandes de build actuelles ne valident que le prototype historique :

    cmake -S third_party/extra/linux -B build-linux-ui -G Ninja -DCMAKE_BUILD_TYPE=Debug
    cmake --build build-linux-ui --verbose
    ctest --test-dir build-linux-ui --output-on-failure

## Cible de remplacement

- SDL2 comme dépendance commune avec le runtime existant.
- SDL2 + SDL_Renderer pour le launcher pré-Play.
- Pas de Dear ImGui, SDL3, SDL_ttf ni Vulkan dans le chemin cible initial.
- Couche de widgets légère, propre au projet, avec navigation clavier/manette et focus visible.
- Le modèle settings/ doit rester testable sans fenêtre, GPU ni ROM.
- Les nouveaux sources natifs Linux restent sous third_party/extra/linux/.
- Le menu in-game est hors de la première étape et bloqué tant qu’un point d’intégration réel n’est pas démontré sans modifier le build Windows.

## Règles de test

Ne pas déclarer un test passé sans consulter sa sortie réelle. Un test ignoré faute d’environnement n’est pas un succès. La CI temporaire actuelle doit elle-même migrer de SDL3/Vulkan vers SDL2 avant de valider la cible révisée.
