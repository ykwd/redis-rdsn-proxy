namespace cpp redisproxy

struct batch_string
{
    1:list<string> values;
}

service redis {
    string write(1: string command);
    string read(1: string command);
    
    batch_string batch_write(1: batch_string commands);
    batch_string batch_read(1: batch_string commands);
}