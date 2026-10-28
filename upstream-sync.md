# Synchronisation upstream — DQIX Enhanced

Date de référence : 2026-10-28

## Sources comparées

- Fork avant synchronisation : `rakmahefa/dqix-enhanced`, commit `4320789d4c16880f08fa9852510e9489146ff789` (merge de la GUI AppImage).
- Upstream : `sackchief/dqix-enhanced`, commit `b85da5d6f8a5580f737e5f96418f4b07a8083591` (v32 et roadmap v33).
- Ancêtre commun : `bcba9c2821c2d540e26e617a20152ad7823307d0`.
- Écart avant synchronisation : fork +30 commits spécifiques ; upstream +51 commits.
- Référence du workflow AppImage amont : appimagetool 1.9.0, SHA-256 `46fdd785094c7f6e545b61afcfb0f3d98d8eab243f644b4b17698c01d06083d1`.

## Changements upstream importants récupérés

La synchronisation récupère le travail v32 : support de la ROM européenne et du menu Language ; `tools/region.py` et wrappers de fonctions ; lecture de configuration sans dépendance dure à PyYAML ; corrections et améliorations du rendu, du timing, des contrôles et du widescreen ; HD text, menu Mods, texture/music packs, export musical et Discord Rich Presence ; build qui signale mieux les étapes en échec ; documentation et notes de release v32.

Les versions upstream de `packaging/linux/AppRun`, `gui.py`, `environment.yml`, du fichier desktop et de l'icône correspondent aux blobs du fork avant synchronisation. Le workflow AppImage upstream est préféré, notamment pour son appimagetool 1.9.0 explicitement versionné et hashé.

## Éléments propres au fork conservés

- `.github/workflows/release-appimage.yml` : workflow de publication manuelle spécifique au fork, absent de l'upstream.
- `appimage-audit.md` et `audit-files.md` : audits détaillés en français, actualisés pour le v32.
- `spec.md` : plan d'optimisation et hardening.
- `upstream-sync.md` : cette trace de synchronisation.

## Politique pour la suite

Ne pas cherry-picker de gros fichiers du runtime v31 sur v32. Pour les synchronisations suivantes, comparer les commits, garder l'upstream comme source de vérité pour le cœur du jeu et isoler les extensions du fork dans des fichiers/commits faciles à rebaser. La GUI AppImage, les filtres de confidentialité et le workflow de release peuvent ensuite être améliorés par des PR dédiées.

Cette synchronisation ne prouve pas à elle seule que tous les tests CI passent ou que le jeu fonctionne sur une configuration Linux réelle. Le statut CI et les vérifications manuelles doivent être ajoutés à l'audit quand ils seront disponibles.

## Contrôle de reprise — 2026-10-28

Le contrôle distant après le handoff confirme que sackchief/dqix-enhanced/main est toujours sur b85da5d6f8a5580f737e5f96418f4b07a8083591. rakmahefa/dqix-enhanced/main reste sur le merge #4 (3a84fe776dfed59421987722073d03a438a3ab56), 32 commits en avance et 0 en retard. Aucune nouvelle synchronisation upstream n'a été faite. L'échec de packaging observé ensuite est propre au workflow AppImage du fork : desktop-file-utils manquait pour fournir desktop-file-validate ; voir le PR #6 et appimage-audit.md.