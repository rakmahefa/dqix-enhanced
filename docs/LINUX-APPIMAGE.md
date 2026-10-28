# Audit AppImage Linux — DQIX Enhanced

Ce document décrit le constructeur Linux AppImage présent dans le fork. Les modifications du jeu et les notes v32 restent celles de l'upstream. Le fork conserve en plus `.github/workflows/release-appimage.yml`, qui permet de publier manuellement un artefact CI validé.

La version AppImage est un **constructeur**, pas un jeu précompilé : l'utilisateur fournit sa ROM et le build complet se fait localement. N'inclure aucune ROM, sauvegarde, extraction, code généré depuis la ROM ou dossier `game/` dans une AppImage ou un artefact.

L'audit détaillé est dans [appimage-audit.md](../appimage-audit.md), l'inventaire des fichiers dans [audit-files.md](../audit-files.md) et le plan de sécurisation/optimisation dans [spec.md](../spec.md). La CI construit l'AppImage sans ROM et teste la toolchain ; le build réel du jeu doit rester local avec une ROM personnelle.
