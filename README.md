# Drupal Build

This is a complete rip off of [Kraftwagen's](http://kraftwagen.org/) `kw-apply-module-dependencies` command.

## Usage

`drush build [--disable] [--uninstall] [environment]`

## Specifying Dependencies

To specify dependencies for a profile or a module, use the property `env_dependencies[environment]` in the `info` file. An example is:

```ini
...
env_dependencies[production][] = admin_menu
env_dependencies[production][] = update
...
```

## Specifying Environments

The environment is determined in this order:

1. argument passed
2. `DRUPAL_BUILD` environment variable e.g. `DRUPAL_BUILD="local" drush build`
3. `config_path/environment` file e.g. sites/default/environment

For the third option, `environment` file will simply contain a string which would represent the environment.
