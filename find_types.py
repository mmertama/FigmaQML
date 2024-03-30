import re


def scan(file):
     with open(file, 'r') as f:
        names = set()
        for line in f.readlines():
            m = re.search(r'"\s*([a-zA-Z_][a-zA-Z_0-9]*)\s*{', line)
            if m:
                names.add(m[1])
        for v in names:
            print(v)

if __name__ == "__main__":
    scan('src/figmaparser.cpp')
