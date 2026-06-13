# Moteur Physique de Particules

Faire intéragir un fluide de 100,000 particules entre elles sans faire s'effondrer ton "framerate".
Tips: Utilise le partitionnement spatial et pas de graphismes juste du calcul

## Règles Métiers

1. Arène spatiale: La simulation se déroule dans une zone 2D fermée (Bounding Box) de largeur $W = 1000$ et
   hauteur $H = 1000$.
2. Cinématique: A chaque itération, la position est mise a jour par la vélocité (`x += dx`, `y += dy`).
3. Boundary Conditions: Si une particule franchit les limites de l'arène sa position est ramenée a sa limite et sa
   vélocité sur cet axe est inversée et amortie de 20%.
4. Rayon d'action: Chaque particule possède un rayon physique fixe $R = 2.0$.
5. Répulsion (Collision): Pour chaque paire de particules (A et B), si la distance au carré entre elles est inférieure
   a $(2R)^2$, elles entrent en collision.
    - Formule de la force: calcule le vecteur de pénétration entre A et B
    - Résolution: écarte les deux particules le long de ce vecteur pour qu'elles ne se chevauchent plus (chacune recule
      de la moitié de la distance de pénétration). Ensuite tu modifies leurs vélocités (`dx`, `dy`) pour simuler un
      rebond elastique simple (addition d'un vecteur de répulsion proportionnel au chevauchement).

## Contraintes Systemes

1. Interdiction de faire une boucle de 100k particules imbriquées dans une autre boucle de 100k, tu dois implémenter une
   Grille Spatiale (Uniform Spatial Grid).
2. Utilise l'allocator que tu as fait comme dans l'ecs
3. Indexation "In-Place": Tu dois construire ta grille spatiale uniquement avec des tableaux plats pré-alloués dans ton
   Arena.
4. A chaque iteration tu dois viter ta grille (remette `cell_head` a -1) et reinserer 100k particules dedans en un seul
   passage.

## Livrable

Generation de 100k particules avec des positions et vélocités initiales aléatoires au centre de la grid et faire tourner
1000 frames de simulation.
si tu dépasses les 100ms ton cpu attend la mémoire.

