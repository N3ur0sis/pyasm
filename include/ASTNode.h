struct ASTNode {
    std::string type;
    std::string value;
    std::vector<std::shared_ptr<ASTNode>> children;
    std::string line = ""; // Changed from "0" to an empty string

    // ...existing fields and member functions...
};
