#pragma once
#define ERRORMANAGER_H

#include <string>
#include <queue>
#include <iostream>

class ErrorManager {
public:
    // Ajouter un message d'erreur à la file
    void addError(const std::string& errorMessage);

    // Afficher tous les messages d'erreur et vider la file
    void displayErrors() const;

    // Vérifier si la file contient des erreurs
    bool hasErrors() const;

private:
    std::queue<std::string> errorQueue; // File pour stocker les messages d'erreur
};