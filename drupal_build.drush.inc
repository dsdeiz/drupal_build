<?php

/**
 * Configuration file.
 */
define('ENVIRONMENT_FILE', 'environment');

/**
 * Implements hook_drush_command.
 *
 */
function drupal_build_drush_command() {
  $items['build'] = array(
    'description' => dt('Build drupal dependencies.'),
    'arguments' => array(
      'environment' => dt("The environment to use for environment specific dependencies. Defaults to 'production'."),
    ),
    'options' => array(
      'disable' => array(
        'description' => dt('Pass this option if you wish to disable modules that are not dependent on the profile.'),
        'value' => 'optional',
      ),
      'uninstall' => array(
        'description' => dt('Pass this option if you wish to uninstall disabled modules.'),
        'value' => 'optional',
      )
    ),
  );

  return $items;
}

/**
 * Command callback: Build module dependencies.
 *
 * This is a complete rip off from kraftwagen. The only change here is that the
 * environment argument is taken from the following order:
 *
 *  - argument passed
 *  - DRUPAL_BUILD environment variable
 *  - config_path/environment file
 *
 * @see https://github.com/kraftwagen/kraftwagen/blob/master/dependencies.module.apply.kw.inc#L15
 * @see install_profile_modules()
 */
function drush_drupal_build_build($env = '') {
  // Try to determine the environment we are building.
  if (strlen($env) === 0) {
    $environment = getenv('DRUPAL_BUILD');
    if (!$environment) {
      $conf_path = './' . conf_path() . '/environment';
      if (file_exists($conf_path)) {
        $environment = trim(file_get_contents($conf_path));
      }
      else {
        $environment = 'production';
      }
    }
  }
  else {
    $environment = $env;
  }

  // Get the data we need. This is a excerpt of system_rebuild_module_data with
  // the environment specific dependencies logic inserted.
  $modules = _system_rebuild_module_data();
  ksort($modules);
  system_get_files_database($modules, 'module');
  system_update_files_database($modules, 'module');

  // Inject enviroment specific dependencies into the dependencies.
  foreach ($modules as $key => $module) {
    if (!empty($module->info['env_dependencies'][$environment])) {
      foreach ($module->info['env_dependencies'][$environment] as $dependency) {
        $module->info['dependencies'][] = $dependency;
      }
    }
  }

  // Resolve dependencies and find out in which order to install modules.
  $files = _module_build_dependencies($modules);

  $profile = drupal_get_profile();

  // If we can't gather data about the install profile, we can't do anything.
  if (!isset($files[$profile])) {
    return drush_set_error(dt('Could not find dependencies for profile !profile.', array('!profile' => $profile)));
  }

  // Temporarily unrequire the profile since it will be a 'module' that will be
  // enabled last.
  $files[$profile]->info['required'] = FALSE;

  // Find all the required modules.
  $required = array();
  foreach ($files as $name => $info) {
    if (!empty($info->info['required']) && !isset($required[$name])) {
      $required[$name] = $info->sort;
      if (!empty($info->requires)) {
        foreach (array_keys($info->requires) as $dependency) {
          if (!isset($required[$dependency])) {
            if (!isset($files[$dependency])) {
              return drush_set_error(dt('Dependency !dependency of required module !module does not exist.', array('!module' => $name, '!dependency' => $dependency)));
            }
            $required[$dependency] = $files[$dependency]->sort;
          }
        }
      }
    }
  }

  arsort($required);

  // Find the modules that the profile depend on (the whole dependency tree).
  $dependencies = array();
  foreach (array_keys($files[$profile]->requires) as $dependency) {
    if (!isset($required[$dependency])) {
      if (!isset($files[$dependency])) {
        return drush_set_error(dt('Dependency !dependency of the install profile !profile does not exist.', array('!dependency' => $dependency, '!profile' => $profile)));
      }

      $dependencies[$dependency] = $files[$dependency]->sort;
    }
  }
  arsort($dependencies);

  // Enable the required modules and dependencies.
  $expected = array_merge(array_keys($required), array_keys($dependencies));
  $result = module_enable($expected);
  if (!$result) {
    return drush_set_error(dt('An unexpected error occured while trying to enable modules !modules', array('!modules', implode(', ', $expected))));
  }

  if (drush_get_option('disable')) {
    $unexpected = array();
    foreach (module_list(TRUE) as $module) {
      if (!in_array($module, $expected) && $module != $profile) {
        if (!isset($files[$module])) {
          drush_log(dt('Could not find dependencies for currently enabled module !module.', array('!module' => $module)), 'error');
        }

        $unexpected[$module] = $files[$module]->sort;
      }
    }

    asort($unexpected);
    $unexpected = array_keys($unexpected);

    // Disable the unexpected modules.
    if ($unexpected) {
      module_disable($unexpected);
    }
  }

  if (drush_get_option('uninstall')) {
    // Check for modules that are still installed even though they are not
    // expected.
    $orphan_installs = array();
    include_once DRUPAL_ROOT . '/includes/install.inc';
    foreach ($files as $name => $info) {
      if ($name != $profile && !in_array($name, $expected)) {
        if (drupal_get_installed_schema_version($name) != SCHEMA_UNINSTALLED) {
          $orphan_installs[$name] = $info->sort;
        }
      }
    }
    asort($orphan_installs);
    $orphan_installs = array_keys($orphan_installs);

    // Remove the hanging installs.
    if ($orphan_installs) {
      $result = drupal_uninstall_modules($orphan_installs, FALSE);
      if (!$result) {
        return drush_set_error(dt('An unexpected error occured while trying to uninstall modules !modules', array('!modules', implode(', ', $orphan_installs))));
      }
    }
  }
}
