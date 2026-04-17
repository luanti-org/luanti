import static android.provider.DocumentsContract.Document;
import static android.provider.DocumentsContract.Root;

import android.database.Cursor;
import android.database.MatrixCursor;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsProvider;

import java.io.File;
import java.io.FileNotFoundException;

import net.minetest.minetest.Utils;
import net.minetest.minetest.R;

public class LuantiDocumentsProvider extends DocumentsProvider {
  private String externalAppDataDirectory;

  private static final String[] DEFAULT_ROOT_PROJECTION = new String[] {Root.COLUMN_ROOT_ID, Root.COLUMN_MIME_TYPES, Root.COLUMN_FLAGS, Root.COLUMN_ICON, Root.COLUMN_TITLE, Root.COLUMN_DOCUMENT_ID};

  private static final String[] DEFAULT_DOCUMENT_PROJECTION = new String[] {Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE, Document.COLUMN_LAST_MODIFIED, Document.COLUMN_FLAGS, Document.COLUMN_SIZE};

  @Override
  public boolean onCreate() {
	externalAppDataDirectory = Utils.getUserDataDirectory(getContext()).getAbsolutePath();
    return true;
  }

  @Override
  public Cursor queryRoots(String[] projection) throws FileNotFoundException {
    final MatrixCursor result = new MatrixCursor(resolveRootProjection(projection));
    final MatrixCursor.RowBuilder appDataRootRow = result.newRow();

    appDataRootRow.add(Root.COLUMN_ROOT_ID, externalAppDataDirectory);
    appDataRootRow.add(Root.COLUMN_MIME_TYPES, "*/*");
    appDataRootRow.add(Root.COLUMN_FLAGS, 0);
    appDataRootRow.add(Root.COLUMN_ICON, R.mipmap.ic_launcher);
    appDataRootRow.add(Root.COLUMN_TITLE, getContext().getString(R.string.documents_provider_root_title));
    appDataRootRow.add(Root.COLUMN_DOCUMENT_ID, externalAppDataDirectory);

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
      } else {
        childDocumentRow.add(Document.COLUMN_MIME_TYPE, "*/*");
      }
      childDocumentRow.add(Document.COLUMN_LAST_MODIFIED, child.lastModified());
      childDocumentRow.add(Document.COLUMN_FLAGS, 0);
      childDocumentRow.add(Document.COLUMN_SIZE, child.length());
    }
    return result;
  }

  @Override
  public Cursor queryDocument(String documentId, String[] projection) throws FileNotFoundException {
    final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
    final File document = new File(documentId);

    final MatrixCursor.RowBuilder documentRow = result.newRow();

    documentRow.add(Document.COLUMN_DOCUMENT_ID, document.getAbsolutePath());
    documentRow.add(Document.COLUMN_DISPLAY_NAME, document.getName());
    if (document.isDirectory()) {
      documentRow.add(Document.COLUMN_MIME_TYPE, Document.MIME_TYPE_DIR);
    } else {
      documentRow.add(Document.COLUMN_MIME_TYPE, "*/*");
    }
    documentRow.add(Document.COLUMN_LAST_MODIFIED, document.lastModified());
    documentRow.add(Document.COLUMN_FLAGS, 0);
    documentRow.add(Document.COLUMN_SIZE, document.length());

    return result;
  }

  @Override
  public ParcelFileDescriptor openDocument(final String documentId, final String mode, CancellationSignal signal) throws UnsupportedOperationException, FileNotFoundException {
    final File file = new File(documentId);

    if (mode != "r") {
      throw new UnsupportedOperationException("Only r mode (MODE_READ_ONLY) is supported");
    }

    return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
  }

  // Helper methods
  private static String[] resolveRootProjection(String[] projection) {
    if (projection == null) {
      return DEFAULT_ROOT_PROJECTION;
    } else {
      return projection;
    }
  }

  private static String[] resolveDocumentProjection(String[] projection) {
    if (projection == null) {
      return DEFAULT_DOCUMENT_PROJECTION;
    } else {
      return projection;
    }
  }
}
