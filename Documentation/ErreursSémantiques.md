# Erreurs sémantiques traitées et restant à traiter

// TODO : Uniformiser les erreurs sémantiques (forme, langue...)

## Erreurs sémantiques Gérées :

- Fonction redéfinies avec le même nom (quelque soit la signature)
- Fonction définie avec un nom réservé (built-in) : range, list, len, print
- Fonction utilisée sans être déclarée
- Fonction utilisées avec la bonne arité des paramètres
- Fonction built-in list, len et range utilisées avec un et un seul paramètre
- Paramètres de fonction : que des noms différents

- shadowing : une variable de boucle ne peut avoir le même nom qu'une variable précédemment définie
- shadowing : une variable locale à une fonction ne doit pas avoir le même nom qu'un paramètre
- shadowing : une variable dans une boucle for ne doit pas avoir le même nom que la variable de boucle

- Vérifie que tout "return" se situe au sein d'une fonction et pas en dehors de toute fonction

- Division par 0 interdite

Erreurs de type :
- Unary Operation utilisée sur autre chose qu'un int (-"String" par exemple)
- Opération arithmétiques utilisée sur des opérandes de types différents
- Types autorisés pour les opérandes d'une opération arithmétique :
    - Premier opérande : Int ou String
    - Deuxième opérande : Int, String ou List
- Sub operation seulement sur Int
- Mul operation seulement sur Int
- Integer Division operation seulement sur Int
- Modulo operation seulement sur Int

## Erreurs sémantiques à traiter :

- détection du numéro de ligne dans l'analyse sémantique et dans la création des TDS
- unused parameter
- utilisation correcte des fonction built-in : range, len, list

- Paramètres de fonction : que des noms différents : présent mais non fonctionnel

- variable utilisée sans initialisation : présent mais non fonctionnel

- Fonction print : utilisation possible avec un nombre quelconque (>0) de paramètres
--> Lignes 51-69 fichier [semanticAnalyser](../src/semanticAnalyzer.cpp) à corriger

- Vérifications à faire ligne 236 à 260 fichier [codeGenerator](../src/codeGenerator.cpp) -> Il y a des problèmes de logique avec les types (type0 != type 1 mais type 1 autorise List et pas type0 ?)

- Lignes 662 à 671 [codeGenerator](../src/codeGenerator.cpp) : est-ce qu'on autorise l'écrasement de la valeur d'une variable, quelque soit le type (ancien et nouveau) ? Actuellement une variable de type int ne peut recevoir un String par exemple.

- Lignes 720 à 732 [codeGenerator](../src/codeGenerator.cpp) : est-ce que les vérifications sont à revoir ou à supprimer (utilisation de range sur Int) ?

## Définition de variable :

- une variable définie avec une certaine valeur et un certain type va simplement écraser la variable précédente
ex :
a =6
...
a = "Hello"

-> pas d'erreur, seulement une redef de a

## Erreurs sémantiques dont le traitement est particulier

- "utilisation d'une variable non déclarée" : 
lors de la génération de code, si on constate que la variable qu'on veut utiliser est de type "auto" dans la table des symboles (type auto correspondant à un type "unknown"), alors cela signifie que cette variable n'a jamais été initialisée avant l'utilisation, donc on soulèvera une erreur sémantique à ce moment. Si une variable est initialisée à un moment, alors son type est inféré et inscrit dans la table des symboles et donc lors de son utilisation, la génération de code va détecter un type précis et ne soulèvera pas d'erreur.

- "Utilisation d'une variable avec un type incorrect" : 
idem, lors de la génération de code

