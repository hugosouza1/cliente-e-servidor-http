# Projeto de Redes: Cliente e Servidor HTTP
**Desenvolvedor:** Hugo Alves Azevedo de Souza

## Execução:
  Sobre o Cliente HTTP(navegador):
  Para compilar:
  ```
  make build_cliente
  ``` 
  Para compilar e executar:
  ```
  make cliente URL=http://exemplo.com/isso.html
  ```
  Caso não coloque uma URL, o programa será executado por padrão buscando index.html. 

  ***
  Sobre o Servidor HTTP:
  Para compilar:
  ```
  make build_server
  ``` 
  Para compilar e executar:
  ```
  make server
  ```

## Funcionalidades:
  **Cliente:**

  * Todas as requisções serão salvas na pasta "solicitados". Caso a pasta não exista, será criado uma.
  * Em caso de falha na requisição, será impresso o erro no terminal.
  * A URL não pode haver espaços vazios

***
  **Servidor:**

* Ao ser solicitado um arquivo inexistente, será listado os arquivos disponíveis. 
* Todo acesso proibido (que não seja somente a pasta www) será bloqueado e retornado mensagem de erro.
* Todos os envios tem cabeçalho de protocolo.