# Audit AppImage Linux — DQIX Enhanced

Ce document décrit le constructeur Linux AppImage présent dans le fork. Les modifications du jeu et les notes v32 restent celles de l'upstream. Le fork conserve en plus `.github/workflows/release-appimage.yml`, qui permet de publier manuellement un artefact CI validé.

La version AppImage est un **constructeur**, pas un jeu précompilé : l'utilisateur fournit sa ROM et le build complet se fait localement. N'inclure aucune ROM, sauvegarde, extraction, code généré depuis la ROM ou dossier `game/` dans une AppImage ou un artefact.

L'audit détaillé est dans [appimage-audit.md](../appimage-audit.md), l'inventaire des fichiers dans [audit-files.md](../audit-files.md) et le plan de sécurisation/optimisation dans [spec.md](../spec.md). La CI construit l'AppImage sans ROM et teste la toolchain ; le build réel du jeu doit rester local avec une ROM personnelle.

## Journal de diagnostic du build

Le constructeur consigne des lignes [timing] : copie des sources dans le workspace, extraction, détection de région, génération ARM9/ARM7, compilation/link et assemblage. Elles indiquent le temps mural et, selon la phase, le nombre de fichiers et le volume en octets. Ces compteurs facilitent la comparaison de deux exécutions ; ils ne mesurent pas directement le pourcentage CPU ni les I/O physiques.

Pour diagnostiquer une durée locale, comparer des exécutions sur la même machine, avec le même workspace et la même ROM dont vous avez les droits d'utilisation : premier lancement à cache froid puis relancement avec le cache chaud. Ne partagez ni ROM, ni sauvegardes, ni workspace. Les tests CI sans ROM ne mesurent pas la génération complète et ne valident pas le gameplay.

La CI de packaging utilise une liste positive pour le payload de l'AppImage, avec exclusions des dossiers privés et tests de sentinelles synthétiques. En cas de modification de la liste, vérifier la CI et son SHA de commit. Le workflow de publication est un déclenchement manuel avec préflight read-only par défaut ; aucun tag ni release n'est créé par la CI de build.
