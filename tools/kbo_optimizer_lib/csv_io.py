"""Small CSV value helpers shared by optimizer modes."""

def to_int(row, key, default=0):
    try:
        return int(row.get(key, default) or default)
    except ValueError:
        return default
