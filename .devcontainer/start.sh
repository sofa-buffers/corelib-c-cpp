docker run -it --rm --name sofa-dev \
  -v "$(pwd)":/workspace \
  -v claude-config:/root/.claude \
  cpp-devcontainer
