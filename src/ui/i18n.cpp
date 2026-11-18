#include "i18n.h"
#include "drbackup.h"

#include <windows.h>

namespace ui {

namespace {

Lang g_lang = Lang::ES;

const Strings kSpanish = {
    /* appTitle       */ L"DRBackupCreator",
    /* appSubtitle    */ L"Gestor de copias de seguridad de DELTARUNE",

    /* savesLabel     */ L"Carpeta de DELTARUNE",
    /* backupsLabel   */ L"Carpeta de backups",
    /* open           */ L"Abrir",
    /* openFolder     */ L"Abrir carpeta",

    /* slotsSection   */ L"Slots detectados (%d)",
    /* emptySlots     */ L"No se encontraron slots.\nGuarda una partida en DELTARUNE y pulsa F5.",
    /* btnCreate      */ L"Crear backup",
    /* btnBackups     */ L"Ver backups",
    /* btnRefresh     */ L"Actualizar",
    /* btnExit        */ L"Salir",

    /* gameWarning    */ L"DELTARUNE está en ejecución. Ciérralo para evitar corromper tu partida.",
    /* gameOk         */ L"DELTARUNE no está en ejecución.",
    /* statusReady    */ L"Listo.",
    /* refreshed      */ L"Lista actualizada.",

    /* backupOk       */ L"Backup creado: %s",
    /* backupErr      */ L"No se pudo crear el backup.",
    /* noSlots        */ L"No hay slots para respaldar.",
    /* noSaveDir      */ L"No se encontró la carpeta de DELTARUNE.",

    /* colIndex       */ L"#",
    /* colChapter     */ L"Capítulo",
    /* colSlot        */ L"Slot",
    /* colType        */ L"Tipo",
    /* colSize        */ L"Tamaño",
    /* colFile        */ L"Archivo",

    /* typeSave       */ L"GUARDADO",
    /* typeCompletion */ L"COMPLETADO",

    /* backupsTitle   */ L"Backups disponibles",
    /* backupsCount   */ L"%d backups encontrados",
    /* backupsCountOne */ L"%d backup encontrado",
    /* colDate        */ L"Fecha y hora",
    /* colCount       */ L"Slots",
    /* colFolder      */ L"Carpeta",
    /* noBackups      */ L"Todavía no hay backups.",

    /* btnRestore     */ L"Restaurar",
    /* btnDelete      */ L"Eliminar",
    /* btnClose       */ L"Cerrar",

    /* restoreTitle   */ L"Restaurar backup",
    /* restoreText    */ L"Se sobrescribirán tus partidas actuales con el backup del %s.\n\n¿Quieres continuar?",
    /* restoreOk      */ L"Restaurados %d archivos.",
    /* restoreErr     */ L"No se pudo restaurar el backup.",

    /* deleteTitle    */ L"Eliminar backup",
    /* deleteText     */ L"El backup del %s se enviará a la Papelera de reciclaje.\n\n¿Quieres continuar?",
    /* deleteOk       */ L"Backup enviado a la Papelera de reciclaje.",
    /* deleteErr      */ L"No se pudo eliminar el backup.",

    /* selectBackup   */ L"Selecciona un backup primero.",
    /* openErr        */ L"No se pudo abrir la carpeta.",

    /* aboutTitle     */ L"Acerca de DRBackupCreator",
    /* aboutBody      */ L"DRBackupCreator %s\n\nPort nativo Win32 del proyecto de DeltaHelpers.\nLicencia GPL-3.0.\n\nhttps://github.com/deltahelpers/DRBackupCreator\nhttps://deltahelpers.github.io",
    /* btnGithub      */ L"GitHub",
    /* btnWebsite     */ L"Sitio web",

    /* langTitle      */ L"Idioma / Language",
    /* langPrompt     */ L"Elige el idioma de la interfaz.\nChoose the interface language.",
    /* btnSpanish     */ L"Español",
    /* btnEnglish     */ L"English",

    /* yes            */ L"Sí",
    /* no             */ L"No",
    /* cancel         */ L"Cancelar",
    /* statusSlots    */ L"%d slots",

    /* settingsTitle   */ L"Ajustes",
    /* settingsSubtitle*/ L"Backup automático, tema y configuración",
    /* scheduleSection */ L"Backup automático",
    /* schedulePrompt  */ L"Crea un backup todos los días a las:",
    /* scheduleOn      */ L"Programado todos los días a las %s.",
    /* scheduleOff     */ L"Sin programar.",
    /* btnScheduleSave */ L"Guardar hora",
    /* btnScheduleOff  */ L"Desactivar",
    /* themeSection    */ L"Tema",
    /* themeCurrent    */ L"Tema actual: %s",
    /* btnThemeLoad    */ L"Cargar…",
    /* btnThemeSave    */ L"Guardar como…",
    /* btnThemeReset   */ L"Predeterminado",
    /* configSection   */ L"Configuración",
    /* btnConfigImport */ L"Importar…",
    /* btnConfigExport */ L"Exportar…",
    /* restartHint     */ L"El tamaño de la ventana se aplica al reiniciar.",
    /* themeSaved      */ L"Tema aplicado.",
    /* themeResetDone  */ L"Tema predeterminado restaurado.",
    /* configImported  */ L"Configuración importada.",
    /* settingsSaved   */ L"Hora guardada.",
    /* filterTheme     */ L"Temas de DRBackupCreator",
    /* filterConfig    */ L"Configuraciones de DRBackupCreator",
    /* filterAll       */ L"Todos los archivos",
    /* scheduleError   */ L"No se pudo programar: %s",
    /* themeError      */ L"No se pudo cargar el tema: %s",
};

const Strings kEnglish = {
    /* appTitle       */ L"DRBackupCreator",
    /* appSubtitle    */ L"DELTARUNE backup manager",

    /* savesLabel     */ L"DELTARUNE folder",
    /* backupsLabel   */ L"Backups folder",
    /* open           */ L"Open",
    /* openFolder     */ L"Open folder",

    /* slotsSection   */ L"Detected slots (%d)",
    /* emptySlots     */ L"No slots found.\nSave a game in DELTARUNE and press F5.",
    /* btnCreate      */ L"Create backup",
    /* btnBackups     */ L"Show backups",
    /* btnRefresh     */ L"Refresh",
    /* btnExit        */ L"Exit",

    /* gameWarning    */ L"DELTARUNE is running. Close it to avoid corrupting your save file.",
    /* gameOk         */ L"DELTARUNE is not running.",
    /* statusReady    */ L"Ready.",
    /* refreshed      */ L"List refreshed.",

    /* backupOk       */ L"Backup created: %s",
    /* backupErr      */ L"Could not create the backup.",
    /* noSlots        */ L"There are no slots to back up.",
    /* noSaveDir      */ L"DELTARUNE save folder not found.",

    /* colIndex       */ L"#",
    /* colChapter     */ L"Chapter",
    /* colSlot        */ L"Slot",
    /* colType        */ L"Type",
    /* colSize        */ L"Size",
    /* colFile        */ L"File",

    /* typeSave       */ L"SAVE",
    /* typeCompletion */ L"COMPLETION",

    /* backupsTitle   */ L"Available backups",
    /* backupsCount   */ L"%d backups found",
    /* backupsCountOne */ L"%d backup found",
    /* colDate        */ L"Date & time",
    /* colCount       */ L"Slots",
    /* colFolder      */ L"Folder",
    /* noBackups      */ L"No backups yet.",

    /* btnRestore     */ L"Restore",
    /* btnDelete      */ L"Delete",
    /* btnClose       */ L"Close",

    /* restoreTitle   */ L"Restore backup",
    /* restoreText    */ L"Your current saves will be overwritten with the backup from %s.\n\nDo you want to continue?",
    /* restoreOk      */ L"Restored %d files.",
    /* restoreErr     */ L"Could not restore the backup.",

    /* deleteTitle    */ L"Delete backup",
    /* deleteText     */ L"The backup from %s will be moved to the Recycle Bin.\n\nDo you want to continue?",
    /* deleteOk       */ L"Backup moved to the Recycle Bin.",
    /* deleteErr      */ L"Could not delete the backup.",

    /* selectBackup   */ L"Select a backup first.",
    /* openErr        */ L"Could not open the folder.",

    /* aboutTitle     */ L"About DRBackupCreator",
    /* aboutBody      */ L"DRBackupCreator %s\n\nNative Win32 port of the DeltaHelpers project.\nGPL-3.0 license.\n\nhttps://github.com/deltahelpers/DRBackupCreator\nhttps://deltahelpers.github.io",
    /* btnGithub      */ L"GitHub",
    /* btnWebsite     */ L"Website",

    /* langTitle      */ L"Idioma / Language",
    /* langPrompt     */ L"Elige el idioma de la interfaz.\nChoose the interface language.",
    /* btnSpanish     */ L"Español",
    /* btnEnglish     */ L"English",

    /* yes            */ L"Yes",
    /* no             */ L"No",
    /* cancel         */ L"Cancel",
    /* statusSlots    */ L"%d slots",

    /* settingsTitle   */ L"Settings",
    /* settingsSubtitle*/ L"Scheduled backup, theme and configuration",
    /* scheduleSection */ L"Daily backup",
    /* schedulePrompt  */ L"Create a backup every day at:",
    /* scheduleOn      */ L"Scheduled every day at %s.",
    /* scheduleOff     */ L"Not scheduled.",
    /* btnScheduleSave */ L"Save time",
    /* btnScheduleOff  */ L"Turn off",
    /* themeSection    */ L"Theme",
    /* themeCurrent    */ L"Current theme: %s",
    /* btnThemeLoad    */ L"Load…",
    /* btnThemeSave    */ L"Save as…",
    /* btnThemeReset   */ L"Default",
    /* configSection   */ L"Configuration",
    /* btnConfigImport */ L"Import…",
    /* btnConfigExport */ L"Export…",
    /* restartHint     */ L"The window size is applied when the app restarts.",
    /* themeSaved      */ L"Theme applied.",
    /* themeResetDone  */ L"Default theme restored.",
    /* configImported  */ L"Configuration imported.",
    /* settingsSaved   */ L"Time saved.",
    /* filterTheme     */ L"DRBackupCreator themes",
    /* filterConfig    */ L"DRBackupCreator configurations",
    /* filterAll       */ L"All files",
    /* scheduleError   */ L"Could not schedule: %s",
    /* themeError      */ L"Could not load the theme: %s",
};

} // namespace

Lang GetLang()
{
    return g_lang;
}

void SetLang(Lang lang)
{
    g_lang = lang;
}

const Strings& T()
{
    return g_lang == Lang::EN ? kEnglish : kSpanish;
}

std::wstring SettingsPath()
{
    auto dir = getSettingsDir(true);
    if (!dir) return {};
    return (dir->wstring() + L"\\settings.ini");
}

bool LoadLang()
{
    const std::wstring path = SettingsPath();
    if (path.empty()) return false;

    wchar_t buffer[16] = {};
    GetPrivateProfileStringW(L"ui", L"language", L"", buffer, 16, path.c_str());
    if (buffer[0] == L'\0') return false;

    g_lang = (_wcsicmp(buffer, L"en") == 0) ? Lang::EN : Lang::ES;
    return true;
}

void SaveLang(Lang lang)
{
    const std::wstring path = SettingsPath();
    if (path.empty()) return;

    WritePrivateProfileStringW(L"ui", L"language",
                               lang == Lang::EN ? L"en" : L"es",
                               path.c_str());
}

} // namespace ui
