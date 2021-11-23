rm -rf ./dist ./pyminizip.egg-info && python3 setup.py sdist && twine upload dist/*
