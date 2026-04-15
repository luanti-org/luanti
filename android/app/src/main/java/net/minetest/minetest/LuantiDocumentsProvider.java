import android.database.Cursor;
import android.database.MatrixCursor;
import android.provider.DocumentsProvider;
import android.provider.DocumentsContract.Root;

import java.io.FileNotFoundException;

public class LuantiDocumentsProvider extends DocumentsProvider {

  private static final String EXTERNAL_APP_DATA_DIRECTORY = Utils.getUserDataDirectory(getContext()).getAbsolutePath();
	
  private static final String[] DEFAULT_ROOT_PROJECTION = new String[] {
    Root.COLUMN_ROOT_ID, Root.COLUMN_MIME_TYPES,
    Root.COLUMN_FLAGS, Root.COLUMN_ICON, Root.COLUMN_TITLE,
	Root.COLUMN_DOCUMENT_ID};
	
  @Override
  public Cursor queryRoots(String[] projection) throws FileNotFoundException {
	// Let's see what happens if I ignore projection
    final MatrixCursor result = new MatrixCursor(DEFAULT_ROOT_PROJECTION /*resolveRootProjection(projection)*/);
	final MatrixCursor.RowBuilder appDataRootRow = result.newRow();

	appDataRootRow.add(Root.COLUMN_ROOT_ID, EXTERNAL_APP_DATA_DIRECTORY);
	appDataRootRow.add(Root.COLUMN_MIME_TYPES, "*/*");
	appDataRootRow.add(Root.COLUMN_FLAGS, Root.FLAG_SUPPORTS_CREATE | Root.FLAG_SUPPORTS_SEARCH);
    appDataRootRow.add(Root.COLUMN_ICON, R.mipmap.ic_launcher);
	appDataRootRow.add(Root.COLUMN_TITLE, getContext().getString(R.string.documents_provider_root_title));
	appDataRootRow.add(Root.COLUMN_DOCUMENT_ID, EXTERNAL_APP_DATA_DIRECTORY);
  }

  // helper method
  private String[] resolveRootProjection(String[] projection) {
    if (projection == null) {
		return DEFAULT_ROOT_PROJECTION;
	} else {
		return projection;
	}
  }
	
}
