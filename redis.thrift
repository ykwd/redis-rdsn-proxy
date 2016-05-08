namespace cpp redisproxy

service redis {
    string write(1: string command);
    string read(1: string command);
}