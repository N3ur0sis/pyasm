#include <queue>
#include <string>
#include <iostream>
#include "errorManager.h"

// Définitions de couleurs ANSI
#define RESET "\033[0m"
#define RED "\033[31m"
#define BLUE "\033[34m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"

void ErrorManager::addError(const Error& error) {
    errorQueue.push(error);
}

void ErrorManager::displayErrors() const {
    if (errorQueue.empty()) {
        std::cout << GREEN << "No errors to display." << RESET << std::endl;
        return;
    }

    std::cout << BOLD << UNDERLINE << "\nErrors found:\n" << RESET << std::endl;

    std::queue<Error> tempQueue = errorQueue;
    while (!tempQueue.empty()) {
        const Error& error = tempQueue.front();

        // Sélection de la couleur en fonction du type d'erreur
        std::string errorColor;
        if (error.type == "Syntax") {
            errorColor = RED; // Rouge pour les erreurs Syntaxiques
        } else if (error.type == "Lexical") {
            errorColor = BLUE; // Bleu pour les erreurs Lexicales
        } else {
            errorColor = GREEN; // Vert par défaut pour d'autres types (optionnel)
        }

        // Affichage coloré des erreurs
        std::cout << errorColor << "[" << error.type << " error]" << RESET
                  << " " << YELLOW << "Line " << error.line << RESET
                  << ": " << error.message << std::endl;

        tempQueue.pop();

        std::cout << "----------------------------------------------------------------------------------------------------------------------------" 
                  << std::endl;
    }
}

bool ErrorManager::hasErrors() const {
    return !errorQueue.empty();
}
