import android.database.Cursor;
import android.database.MatrixCursor;
import android.provider.DocumentsProvider;
import android.provider.DocumentsContract.Root;

import java.io.File;
import java.io.FileNotFoundException;

public class LuantiDocumentsProvider extends DocumentsProvider {

  private static final String EXTERNAL_APP_DATA_DIRECTORY = Utils.getUserDataDirectory(getContext()).getAbsolutePath();
	
  private static final String[] DEFAULT_ROOT_PROJECTION = new String[] {
    Root.COLUMN_ROOT_ID, Root.COLUMN_MIME_TYPES,
    Root.COLUMN_FLAGS, Root.COLUMN_ICON, Root.COLUMN_TITLE,
	Root.COLUMN_DOCUMENT_ID};

  private static final String[] DEFAULT_DOCUMENT_PROJECTION = new String[] {
    Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME,
    Document.COLUMN_MIME_TYPE, Document.COLUMN_LAST_MODIFIED,
    Document.COLUMN_FLAGS, Document.COLUMN_SIZE,};
	
  @Override
  public Cursor queryRoots(String[] projection) throws FileNotFoundException {
	
    final MatrixCursor result = new MatrixCursor(resolveRootProjection(projection));
	final MatrixCursor.RowBuilder appDataRootRow = result.newRow();

	
	appDataRootRow.add(Root.COLUMN_ROOT_ID, EXTERNAL_APP_DATA_DIRECTORY);
    appDataRootRow.add(Root.COLUMN_MIME_TYPES, "*/*");
	appDataRootRow.add(Root.COLUMN_FLAGS, /*Root.FLAG_SUPPORTS_CREATE |*/ Root.FLAG_SUPPORTS_SEARCH);
    appDataRootRow.add(Root.COLUMN_ICON, R.mipmap.ic_launcher);
	appDataRootRow.add(Root.COLUMN_TITLE, getContext().getString(R.string.documents_provider_root_title));
	appDataRootRow.add(Root.COLUMN_DOCUMENT_ID, EXTERNAL_APP_DATA_DIRECTORY);

	
	return result;
  }

  @Override
  public Cursor queryChildDocuments(String parentDocumentId, String[] projection, String sortOrder) throws FileNotFoundException {

    final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
    final File parent = new File(parentDocumentId);
    for (File child : parent.listFiles()) {
        final MatrixCursor.RowBuilder childDocumentRow = result.newRow();

		childDocumentRow.add(Document.COLUMN_DOCUMENT_ID, child.getAbsolutePath());
		childDocumentRow.add(Document.COLUMN_DISPLAY_NAME, child.getName());
		if (child.isDirectory()) {
		  childDocumentRow.add(Document.COLUMN_MIME_TYPE, Document.MIME_TYPE_DIR);
		else {
          childDocumentRow.add(Document.COLUMN_MIME_TYPE, "*/*");
		}
		childDocumentRow.add(Document.COLUMN_LAST_MODIFIED, child.lastModified());
		childDocumentRow.add(Document.COLUMN_FLAGS, 0);
		childDocumentRow.add(Document.COLUMN_SIZE, child.length());
    }
    return result;
}

  // Helper methods
  private String[] resolveRootProjection(String[] projection) {
    if (projection == null) {
		return DEFAULT_ROOT_PROJECTION;
	} else {
		return projection;
	}
  }

  private String[] resolveDocumentProjection(String[] projection) {
    if (projection == null) {
		return DEFAULT_DOCUMENT_PROJECTION;
	} else {
		return projection;
	}
  }
	
}
