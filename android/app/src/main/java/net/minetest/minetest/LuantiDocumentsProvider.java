import android.database.Cursor;
import android.database.MatrixCursor;
import android.provider.DocumentsProvider;
import android.provider.DocumentsContract.Root;

import java.io.FileNotFoundException;

public class LuantiDocumentsProvider extends DocumentsProvider {

  private static final String[] DEFAULT_ROOT_PROJECTION = new String[] {
    Root.COLUMN_ROOT_ID, Root.COLUMN_MIME_TYPES,
    Root.COLUMN_FLAGS, Root.COLUMN_ICON, Root.COLUMN_TITLE
	Root.COLUMN_SUMMARY, Root.COLUMN_DOCUMENT_ID};
	
  @Override
  public Cursor queryRoots(String[] projection) throws FileNotFoundException {
    
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
