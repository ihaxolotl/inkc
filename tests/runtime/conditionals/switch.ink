VAR foo = 10

{foo:
    - 1: 1
    - 2: 2
    - 3: 3
    - else:
        {foo:
            - 4: 4
            - 5: 5
            - else: Greater than 5!
        }
}
