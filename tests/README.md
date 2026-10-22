# Testes de host

Cada `tests/*.cpp` vira um executável compilado com o g++ do PC, linkado com `firmware/src/*.cpp` (exceto `main.cpp`). Retorno 0 = passou. Rodar: `make -C firmware test`.

Convenção: sem framework; `assert`-style com mensagem e `return 1`. Um arquivo por módulo (`seguranca.cpp`, `politica.cpp`, `at_parser.cpp`, `sprites.cpp`). Testes que precisam de hardware não entram aqui — vão para os critérios [LAB] da spec.
